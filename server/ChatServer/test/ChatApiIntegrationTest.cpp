#include <grpcpp/grpcpp.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ChatApiService.h"
#include "DeliverApiClient.h"
#include "Logger.h"
#include "RedisMgr.h"
#include "im.grpc.pb.h"

namespace {

class ScopedChatApiServer {
public:
    ScopedChatApiServer() {
        auto deliver_client = std::make_shared<DeliverApiClient>();
        service_ = std::make_unique<ChatApiService>(deliver_client);

        grpc::ServerBuilder builder;
        builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port_);
        builder.RegisterService(service_.get());
        server_ = builder.BuildAndStart();
        if (!server_ || port_ <= 0) {
            throw std::runtime_error("failed to start ChatApi test server");
        }
    }

    ~ScopedChatApiServer() {
        if (server_) {
            server_->Shutdown();
        }
    }

    std::unique_ptr<im::ChatApi::Stub> NewStub() const {
        auto channel = grpc::CreateChannel(
            "127.0.0.1:" + std::to_string(port_),
            grpc::InsecureChannelCredentials());
        return im::ChatApi::NewStub(channel);
    }

private:
    int port_ = 0;
    std::unique_ptr<ChatApiService> service_;
    std::unique_ptr<grpc::Server> server_;
};

struct TestContext {
    std::unique_ptr<im::ChatApi::Stub> stub;
    int64_t uid_seed = 0;
    std::unordered_set<int64_t> touched_uids;
    std::vector<std::pair<int64_t, std::string>> idempotent_keys;
};

std::atomic<int64_t> g_uid_seed{
    std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count() % 1000000000LL
};

void CheckStatus(const grpc::Status& status, const std::string& step) {
    if (!status.ok()) {
        throw std::runtime_error(step + " failed: " + status.error_message());
    }
}

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

int64_t NextUid(TestContext& ctx) {
    const auto uid = 100000 + g_uid_seed.fetch_add(1);
    ctx.touched_uids.insert(uid);
    return uid;
}

std::string UniqueSuffix() {
    return std::to_string(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

void CleanupUserInbox(int64_t uid, uint64_t max_seq_hint = 64) {
    auto& redis = RedisMgr::getInstance();
    redis.del("user_inbox_seq:" + std::to_string(uid));
    redis.del("offline_index:" + std::to_string(uid));
    redis.del("ack_seq:" + std::to_string(uid));
    for (uint64_t seq = 1; seq <= max_seq_hint; ++seq) {
        redis.del("offline_msg:" + std::to_string(uid) + ":" + std::to_string(seq));
    }
}

void CleanupIdempotent(int64_t from_uid, const std::string& client_msg_id) {
    RedisMgr::getInstance().del("idempotent:" + std::to_string(from_uid) + ":" + client_msg_id);
}

im::SendMessageResp SendMessage(TestContext& ctx,
                                int64_t from_uid,
                                int64_t to_uid,
                                const std::string& client_msg_id,
                                const std::string& payload,
                                int64_t group_id = 0) {
    im::SendMessageReq req;
    req.set_from_uid(from_uid);
    req.set_to_uid(to_uid);
    req.set_group_id(group_id);
    req.set_client_msg_id(client_msg_id);
    req.set_payload(payload);
    req.set_trace_id("trace-" + client_msg_id);

    im::SendMessageResp resp;
    grpc::ClientContext rpc_ctx;
    CheckStatus(ctx.stub->SendMessage(&rpc_ctx, req, &resp), "SendMessage");
    if (group_id == 0 && to_uid > 0) {
        ctx.idempotent_keys.emplace_back(from_uid, client_msg_id);
    }
    return resp;
}

im::SyncUpResp Sync(TestContext& ctx, int64_t uid, uint64_t last_inbox_seq, uint32_t limit) {
    im::SyncUpReq req;
    req.set_uid(uid);
    req.set_last_inbox_seq(last_inbox_seq);
    req.set_limit(limit);

    im::SyncUpResp resp;
    grpc::ClientContext rpc_ctx;
    CheckStatus(ctx.stub->Sync(&rpc_ctx, req, &resp), "Sync");
    return resp;
}

im::AckUpResp Ack(TestContext& ctx, int64_t uid, uint64_t max_inbox_seq, const std::string& msg_id = "") {
    im::AckUpReq req;
    req.set_uid(uid);
    req.set_msg_id(msg_id);
    req.set_type(im::ACK_RECEIVED);
    req.set_ts_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch()).count());
    req.set_max_inbox_seq(max_inbox_seq);

    im::AckUpResp resp;
    grpc::ClientContext rpc_ctx;
    CheckStatus(ctx.stub->Ack(&rpc_ctx, req, &resp), "Ack");
    return resp;
}

void RunCase(const std::string& name, const std::function<void(TestContext&)>& fn) {
    TestContext ctx;
    ScopedChatApiServer server;
    ctx.stub = server.NewStub();

    try {
        fn(ctx);
        for (const auto uid : ctx.touched_uids) {
            CleanupUserInbox(uid);
        }
        for (const auto& key : ctx.idempotent_keys) {
            CleanupIdempotent(key.first, key.second);
        }
        std::cout << "[PASS] " << name << std::endl;
    } catch (...) {
        for (const auto uid : ctx.touched_uids) {
            CleanupUserInbox(uid);
        }
        for (const auto& key : ctx.idempotent_keys) {
            CleanupIdempotent(key.first, key.second);
        }
        throw;
    }
}

void TestBasicSendSyncAck(TestContext& ctx) {
    const auto from_uid = NextUid(ctx);
    const auto to_uid = NextUid(ctx);
    const auto client_msg_id = "basic-" + UniqueSuffix();

    const auto send_resp = SendMessage(ctx, from_uid, to_uid, client_msg_id, "hello offline");
    Require(send_resp.ok(), "basic send returned not ok");

    const auto sync_resp = Sync(ctx, to_uid, 0, 10);
    Require(sync_resp.ok(), "basic sync returned not ok");
    Require(sync_resp.msgs_size() == 1, "basic sync should return one message");

    const auto& env = sync_resp.msgs(0);
    Require(env.msg_id() == send_resp.msg_id(), "basic msg_id mismatch");
    Require(env.client_msg_id() == client_msg_id, "basic client_msg_id mismatch");
    Require(env.from_uid() == from_uid, "basic from_uid mismatch");
    Require(env.to_uid() == to_uid, "basic to_uid mismatch");
    Require(env.payload() == "hello offline", "basic payload mismatch");
    Require(env.seq_id() > 0, "basic seq_id missing");
    Require(sync_resp.max_inbox_seq() == env.seq_id(), "basic max_inbox_seq mismatch");
    Require(!sync_resp.has_more(), "basic has_more should be false");

    const auto ack_resp = Ack(ctx, to_uid, env.seq_id(), env.msg_id());
    Require(ack_resp.ok(), "basic ack returned not ok");

    const auto sync_after_ack = Sync(ctx, to_uid, 0, 10);
    Require(sync_after_ack.ok(), "basic sync after ack returned not ok");
    Require(sync_after_ack.msgs_size() == 1, "ack should not delete offline message");

    const auto sync_after_cursor_advance = Sync(ctx, to_uid, env.seq_id(), 10);
    Require(sync_after_cursor_advance.ok(), "sync after cursor advance returned not ok");
    Require(sync_after_cursor_advance.msgs_size() == 0,
            "cursor-based sync should suppress already received message");
}

void TestIdempotentSend(TestContext& ctx) {
    const auto from_uid = NextUid(ctx);
    const auto to_uid = NextUid(ctx);
    const auto client_msg_id = "idem-" + UniqueSuffix();

    const auto first = SendMessage(ctx, from_uid, to_uid, client_msg_id, "same payload");
    const auto second = SendMessage(ctx, from_uid, to_uid, client_msg_id, "same payload");

    Require(first.ok(), "idempotent first send failed");
    Require(second.ok(), "idempotent second send failed");
    Require(first.msg_id() == second.msg_id(), "idempotent send returned different msg_id");

    const auto sync_resp = Sync(ctx, to_uid, 0, 10);
    Require(sync_resp.msgs_size() == 1, "idempotent sync should return one message");
    Require(sync_resp.msgs(0).msg_id() == first.msg_id(), "idempotent synced msg_id mismatch");
}

void TestPaginationAndAckProgress(TestContext& ctx) {
    const auto from_uid = NextUid(ctx);
    const auto to_uid = NextUid(ctx);
    const auto prefix = "page-" + UniqueSuffix();

    const auto first = SendMessage(ctx, from_uid, to_uid, prefix + "-1", "p1");
    const auto second = SendMessage(ctx, from_uid, to_uid, prefix + "-2", "p2");
    const auto third = SendMessage(ctx, from_uid, to_uid, prefix + "-3", "p3");

    Require(first.ok() && second.ok() && third.ok(), "pagination sends failed");

    const auto sync_page1 = Sync(ctx, to_uid, 0, 2);
    Require(sync_page1.msgs_size() == 2, "pagination first page should return two messages");
    Require(sync_page1.has_more(), "pagination first page should have_more");
    Require(sync_page1.msgs(0).seq_id() < sync_page1.msgs(1).seq_id(), "pagination seq order broken");

    const auto page1_max = sync_page1.max_inbox_seq();
    const auto sync_page2 = Sync(ctx, to_uid, page1_max, 2);
    Require(sync_page2.msgs_size() == 1, "pagination second page should return one message");
    Require(!sync_page2.has_more(), "pagination second page should not have_more");
    Require(sync_page2.msgs(0).payload() == "p3", "pagination second page payload mismatch");

    const auto ack_resp = Ack(ctx, to_uid, sync_page1.msgs(0).seq_id(), sync_page1.msgs(0).msg_id());
    Require(ack_resp.ok(), "partial ack failed");

    const auto sync_after_partial_ack = Sync(ctx, to_uid, 0, 10);
    Require(sync_after_partial_ack.msgs_size() == 3,
            "ack no-op mode should keep offline messages intact");

    const auto sync_after_partial_cursor = Sync(ctx, to_uid, sync_page1.msgs(0).seq_id(), 10);
    Require(sync_after_partial_cursor.msgs_size() == 2,
            "cursor-based sync should still skip acknowledged range");
    Require(sync_after_partial_cursor.msgs(0).payload() == "p2", "remaining message order mismatch after cursor advance");
    Require(sync_after_partial_cursor.msgs(1).payload() == "p3", "remaining message tail mismatch after cursor advance");
}

void TestSeqMonotonicity(TestContext& ctx) {
    const auto from_uid = NextUid(ctx);
    const auto to_uid = NextUid(ctx);
    const auto prefix = "seq-" + UniqueSuffix();

    for (int i = 0; i < 5; ++i) {
        const auto resp = SendMessage(ctx, from_uid, to_uid,
                                      prefix + "-" + std::to_string(i),
                                      "msg-" + std::to_string(i));
        Require(resp.ok(), "seq monotonicity send failed");
    }

    const auto sync_resp = Sync(ctx, to_uid, 0, 10);
    Require(sync_resp.msgs_size() == 5, "seq monotonicity should return five messages");
    for (int i = 1; i < sync_resp.msgs_size(); ++i) {
        Require(sync_resp.msgs(i - 1).seq_id() + 1 == sync_resp.msgs(i).seq_id(),
                "seq monotonicity broken");
    }
}

void TestInvalidAndUnsupportedInputs(TestContext& ctx) {
    const auto from_uid = NextUid(ctx);

    const auto invalid = SendMessage(ctx, from_uid, 0, "invalid-" + UniqueSuffix(), "bad");
    Require(!invalid.ok(), "invalid to_uid should fail");
    Require(invalid.reason() == "invalid to_uid", "invalid to_uid reason mismatch");

    const auto group = SendMessage(ctx, from_uid, 123456, "group-" + UniqueSuffix(), "group", 999);
    Require(!group.ok(), "unsupported group send should fail");
    Require(!group.reason().empty(), "group failure should return reason");

    const auto stale_ack = Ack(ctx, from_uid, 0, "");
    Require(stale_ack.ok(), "zero-seq ack should be treated as ok");
}

}  // namespace

int main() {
    Logger::init();

    try {
        RunCase("basic_send_sync_ack", TestBasicSendSyncAck);
        RunCase("idempotent_send", TestIdempotentSend);
        RunCase("pagination_and_ack_progress", TestPaginationAndAckProgress);
        RunCase("seq_monotonicity", TestSeqMonotonicity);
        RunCase("invalid_and_unsupported_inputs", TestInvalidAndUnsupportedInputs);
        std::cout << "ChatApiIntegrationTest passed" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ChatApiIntegrationTest failed: " << e.what() << std::endl;
        return 1;
    }
}
