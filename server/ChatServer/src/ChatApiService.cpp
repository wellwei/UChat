#include "ChatApiService.h"
#include "RedisMgr.h"
#include "Logger.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <chrono>
#include <nlohmann/json.hpp>

ChatApiService::ChatApiService(std::shared_ptr<DeliverApiClient> deliver_client)
    : deliver_client_(std::move(deliver_client)) {
}

grpc::ServerUnaryReactor* ChatApiService::SendMessage(
    grpc::CallbackServerContext* context,
    const im::SendMessageReq* request,
    im::SendMessageResp* response) {

    auto* reactor = context->DefaultReactor();
    auto& redis = RedisMgr::getInstance();

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
        LOG_DEBUG("ChatApiService::SendMessage: idempotent hit, from_uid={}, client_msg_id={}",
                  request->from_uid(), request->client_msg_id());
        reactor->Finish(grpc::Status::OK);
        return reactor;
    }

    // 3. Generate SeqID for conversation-level ordering
    std::string conversation_id;
    if (request->group_id() > 0) {
        conversation_id = "group_" + std::to_string(request->group_id());
    } else {
        int64_t min_uid = std::min(request->from_uid(), request->to_uid());
        int64_t max_uid = std::max(request->from_uid(), request->to_uid());
        conversation_id = std::to_string(min_uid) + "_" + std::to_string(max_uid);
    }
    uint64_t seq_id = redis.getNextSeq(conversation_id);

    // 4. PERSIST FIRST: Always store to offline queue as persistent storage
    // This is the source of truth for message delivery
    redis.addToOfflineQueue(request->to_uid(), request->from_uid(),
                             msg_id, ts_ms, request->payload(), seq_id);

    // 5. Return success to sender immediately
    response->set_ok(true);
    response->set_msg_id(msg_id);
    response->set_ts_ms(ts_ms);
    reactor->Finish(grpc::Status::OK);

    // 6. Fire-and-forget: If user is online, push to Gateway asynchronously
    auto presence = redis.getPresence(request->to_uid());
    if (presence) {
        // Build envelope for delivery
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

        im::DeliverToUserReq deliver_req;
        deliver_req.set_to_uid(request->to_uid());
        *deliver_req.mutable_env() = env;
        deliver_req.set_expected_session_ver(presence->session_ver);

        // Fire-and-forget: async delivery, message already persisted
        deliver_client_->DeliverToUserAsync(presence->gateway_addr, deliver_req);

        LOG_DEBUG("ChatApiService::SendMessage: async push to uid={} at gateway={}",
                  request->to_uid(), presence->gateway_addr);
    } else {
        LOG_DEBUG("ChatApiService::SendMessage: to_uid={} is offline, msg stored", request->to_uid());
    }

    return reactor;
}

grpc::ServerUnaryReactor* ChatApiService::Ack(
    grpc::CallbackServerContext* context,
    const im::AckUpReq* request,
    im::AckUpResp* response) {

    auto* reactor = context->DefaultReactor();

    // ACK 处理（目前暂不需要额外操作）
    response->set_ok(true);
    reactor->Finish(grpc::Status::OK);
    return reactor;
}

grpc::ServerUnaryReactor* ChatApiService::Sync(
    grpc::CallbackServerContext* context,
    const im::SyncUpReq* request,
    im::SyncUpResp* response) {

    auto* reactor = context->DefaultReactor();
    auto& redis = RedisMgr::getInstance();

    // Get all offline messages for this user
    auto json_msgs = redis.getOfflineMessages(request->uid());

    // Filter messages based on client's last_seq_map
    const auto& last_seq_map = request->last_seq_map();

    for (const auto& json_str : json_msgs) {
        try {
            auto j = nlohmann::json::parse(json_str);

            // Get conversation_id for this message
            int64_t from_uid = j.value("from_uid", int64_t(0));
            int64_t to_uid = j.value("to_uid", int64_t(0));
            uint64_t seq_id = j.value("seq_id", uint64_t(0));

            // Determine conversation_id
            std::string conversation_id;
            int64_t min_uid = std::min(from_uid, to_uid);
            int64_t max_uid = std::max(from_uid, to_uid);
            conversation_id = std::to_string(min_uid) + "_" + std::to_string(max_uid);

            // Check if client already has this message
            auto it = last_seq_map.find(conversation_id);
            if (it != last_seq_map.end() && seq_id <= it->second) {
                // Client already has this message, skip
                continue;
            }

            // Add message to response
            im::Envelope* env = response->add_msgs();
            env->set_msg_id(j.value("msg_id", ""));
            env->set_client_msg_id(j.value("client_msg_id", ""));
            env->set_from_uid(from_uid);
            env->set_to_uid(to_uid);
            env->set_group_id(j.value("group_id", int64_t(0)));
            env->set_ts_ms(j.value("ts_ms", int64_t(0)));
            env->set_payload(j.value("payload", ""));
            env->set_trace_id(j.value("trace_id", ""));
            if (seq_id > 0) {
                env->set_seq_id(seq_id);
            }
        } catch (...) {
            LOG_WARN("ChatApiService::Sync: failed to parse json for uid={}", request->uid());
        }
    }

    response->set_ok(true);
    LOG_DEBUG("ChatApiService::Sync: uid={} returned {} messages", request->uid(), response->msgs_size());

    reactor->Finish(grpc::Status::OK);
    return reactor;
}
