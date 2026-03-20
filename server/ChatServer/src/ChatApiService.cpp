#include "ChatApiService.h"
#include "RedisMgr.h"
#include "Logger.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <chrono>

ChatApiService::ChatApiService(std::shared_ptr<DeliverApiClient> deliver_client)
    : deliver_client_(std::move(deliver_client)) {
}

grpc::ServerUnaryReactor* ChatApiService::SendMessage(
    grpc::CallbackServerContext* context,
    const im::SendMessageReq* request,
    im::SendMessageResp* response) {

    auto* reactor = context->DefaultReactor();
    auto& redis = RedisMgr::getInstance();

    if (request->group_id() > 0) {
        std::vector<int64_t> member_uids;
        if (!user_service_client_.GetGroupMemberUids(static_cast<uint64_t>(request->from_uid()),
                                                     static_cast<uint64_t>(request->group_id()),
                                                     member_uids)) {
            response->set_ok(false);
            response->set_reason("failed to load group members");
            reactor->Finish(grpc::Status::OK);
            return reactor;
        }

        std::string msg_id = boost::uuids::to_string(boost::uuids::random_generator()());
        int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        auto existing = redis.checkOrSetIdempotent(request->from_uid(), request->client_msg_id(), msg_id);
        if (existing) {
            response->set_ok(true);
            response->set_msg_id(*existing);
            reactor->Finish(grpc::Status::OK);
            return reactor;
        }

        size_t persisted = 0;
        for (const auto member_uid : member_uids) {
            if (member_uid <= 0 || member_uid == request->from_uid()) {
                continue;
            }

            const uint64_t seq_id = redis.getNextInboxSeq(member_uid);
            if (seq_id == 0) {
                LOG_WARN("ChatApiService::SendMessage group fanout seq alloc failed group_id={} to_uid={}",
                         request->group_id(), member_uid);
                continue;
            }

            im::Envelope env;
            env.set_msg_id(msg_id);
            env.set_client_msg_id(request->client_msg_id());
            env.set_from_uid(request->from_uid());
            env.set_to_uid(member_uid);
            env.set_group_id(request->group_id());
            env.set_ts_ms(ts_ms);
            env.set_payload(request->payload());
            env.set_trace_id(request->trace_id());
            env.set_seq_id(seq_id);

            if (!redis.appendOfflineMessage(member_uid, env)) {
                LOG_WARN("ChatApiService::SendMessage group fanout persist failed group_id={} to_uid={}",
                         request->group_id(), member_uid);
                continue;
            }
            ++persisted;

            auto presence = redis.getPresence(member_uid);
            if (presence) {
                im::DeliverToUserReq deliver_req;
                deliver_req.set_to_uid(member_uid);
                *deliver_req.mutable_env() = env;
                deliver_req.set_expected_session_ver(presence->session_ver);
                deliver_client_->DeliverToUser(presence->gateway_addr, deliver_req);
            }
        }

        response->set_ok(persisted > 0);
        response->set_msg_id(msg_id);
        response->set_ts_ms(ts_ms);
        if (persisted == 0) {
            response->set_reason("no group recipients persisted");
        }
        reactor->Finish(grpc::Status::OK);
        return reactor;
    }
    if (request->to_uid() <= 0) {
        response->set_ok(false);
        response->set_reason("invalid to_uid");
        reactor->Finish(grpc::Status::OK);
        return reactor;
    }

    // 1. Generate server_msg_id (UUID) and timestamp
    std::string msg_id = boost::uuids::to_string(boost::uuids::random_generator()());
    int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // 2. Atomic idempotency check-and-set
    // Returns existing msg_id if duplicate, nullopt if new (and sets the key)
    auto existing = redis.checkOrSetIdempotent(request->from_uid(), request->client_msg_id(), msg_id);
    if (existing) {
        response->set_ok(true);
        response->set_msg_id(*existing);
        // LOG_DEBUG("ChatApiService::SendMessage: idempotent hit, from_uid={}, client_msg_id={}",
        //          request->from_uid(), request->client_msg_id());
        reactor->Finish(grpc::Status::OK);
        return reactor;
    }

    // 3. Generate receiver-scoped inbox seq.
    const uint64_t seq_id = redis.getNextInboxSeq(request->to_uid());
    if (seq_id == 0) {
        response->set_ok(false);
        response->set_reason("failed to allocate inbox seq");
        reactor->Finish(grpc::Status::OK);
        return reactor;
    }

    // 4. PERSIST FIRST: write receiver-specific inbox copy.
    im::Envelope env;
    env.set_msg_id(msg_id);
    env.set_client_msg_id(request->client_msg_id());
    env.set_from_uid(request->from_uid());
    env.set_to_uid(request->to_uid());
    env.set_group_id(request->group_id());
    env.set_ts_ms(ts_ms);
    env.set_payload(request->payload());
    env.set_trace_id(request->trace_id());
    env.set_seq_id(seq_id);

    if (!redis.appendOfflineMessage(request->to_uid(), env)) {
        response->set_ok(false);
        response->set_reason("failed to persist message");
        reactor->Finish(grpc::Status::OK);
        return reactor;
    }

    // 5. Return success to sender immediately
    response->set_ok(true);
    response->set_msg_id(msg_id);
    response->set_ts_ms(ts_ms);
    reactor->Finish(grpc::Status::OK);

    // 6. Fire-and-forget: If user is online, push to Gateway asynchronously
    auto presence = redis.getPresence(request->to_uid());
    if (presence) {
        im::DeliverToUserReq deliver_req;
        deliver_req.set_to_uid(request->to_uid());
        *deliver_req.mutable_env() = env;
        deliver_req.set_expected_session_ver(presence->session_ver);

        // Fire-and-forget: async delivery, message already persisted
        deliver_client_->DeliverToUser(presence->gateway_addr, deliver_req);

        LOG_DEBUG("ChatApiService::SendMessage: async push to uid={} at gateway={}",
                  request->to_uid(), presence->gateway_addr);
    } else {
        // LOG_DEBUG("ChatApiService::SendMessage: to_uid={} is offline, msg stored", request->to_uid());
    }

    return reactor;
}

grpc::ServerUnaryReactor* ChatApiService::Ack(
    grpc::CallbackServerContext* context,
    const im::AckUpReq* request,
    im::AckUpResp* response) {

    auto* reactor = context->DefaultReactor();
    auto& redis = RedisMgr::getInstance();

    response->set_ok(redis.ackInboxMessages(request->uid(), request->max_inbox_seq()));
    reactor->Finish(grpc::Status::OK);
    return reactor;
}

grpc::ServerUnaryReactor* ChatApiService::Sync(
    grpc::CallbackServerContext* context,
    const im::SyncUpReq* request,
    im::SyncUpResp* response) {

    auto* reactor = context->DefaultReactor();
    auto& redis = RedisMgr::getInstance();

    const auto batch = redis.getOfflineMessagesBySeq(request->uid(),
                                                     request->last_inbox_seq(),
                                                     request->limit());

    for (const auto& env : batch.messages) {
        *response->add_msgs() = env;
    }

    response->set_ok(true);
    response->set_max_inbox_seq(batch.max_inbox_seq);
    response->set_has_more(batch.has_more);
    LOG_DEBUG("ChatApiService::Sync: uid={} from_seq={} returned {} messages max_seq={} has_more={}",
              request->uid(), request->last_inbox_seq(), response->msgs_size(),
              batch.max_inbox_seq, batch.has_more);

    reactor->Finish(grpc::Status::OK);
    return reactor;
}
