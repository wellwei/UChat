#include "ChatApiService.h"
#include "Logger.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <chrono>

ChatApiService::ChatApiService(std::shared_ptr<PresenceClient> presence_client,
                               std::shared_ptr<DeliverApiClient> deliver_client)
    : presence_client_(std::move(presence_client)),
      deliver_client_(std::move(deliver_client)) {
}

grpc::ServerUnaryReactor* ChatApiService::SendMessage(
    grpc::CallbackServerContext* context,
    const im::SendMessageReq* request,
    im::SendMessageResp* response) {

    auto* reactor = context->DefaultReactor();

    // 1. Idempotency check
    auto existing = presence_client_->CheckIdempotent(request->from_uid(), request->client_msg_id());
    if (existing) {
        response->set_ok(true);
        response->set_msg_id(*existing);
        LOG_DEBUG("ChatApiService::SendMessage: idempotent hit, from_uid={}, client_msg_id={}",
                  request->from_uid(), request->client_msg_id());
        reactor->Finish(grpc::Status::OK);
        return reactor;
    }

    // 2. Generate server_msg_id (UUID) and timestamp
    std::string msg_id = boost::uuids::to_string(boost::uuids::random_generator()());
    int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // 3. Store idempotency key (24h TTL)
    presence_client_->SetIdempotent(request->from_uid(), request->client_msg_id(), msg_id);

    response->set_ok(true);
    response->set_msg_id(msg_id);
    response->set_ts_ms(ts_ms);

    // 4. Query presence
    auto presence = presence_client_->GetPresence(request->to_uid());
    if (!presence) {
        LOG_DEBUG("ChatApiService::SendMessage: to_uid={} is offline", request->to_uid());
        reactor->Finish(grpc::Status::OK);
        return reactor;
    }

    // 5. Build envelope
    im::Envelope env;
    env.set_msg_id(msg_id);
    env.set_client_msg_id(request->client_msg_id());
    env.set_from_uid(request->from_uid());
    env.set_to_uid(request->to_uid());
    env.set_group_id(request->group_id());
    env.set_ts_ms(ts_ms);
    env.set_payload(request->payload());
    env.set_trace_id(request->trace_id());

    // 6. Deliver to gateway (50ms deadline, retry once on failure)
    im::DeliverToUserReq deliver_req;
    deliver_req.set_to_uid(request->to_uid());
    *deliver_req.mutable_env() = env;
    deliver_req.set_expected_session_ver(presence->session_ver);

    im::DeliverToUserResp deliver_resp;
    bool delivered = deliver_client_->DeliverToUser(presence->gateway_addr, deliver_req, deliver_resp);

    if (!delivered) {
        // Retry once: re-query presence in case of gateway migration
        auto presence2 = presence_client_->GetPresence(request->to_uid());
        if (presence2 && presence2->gateway_addr != presence->gateway_addr) {
            deliver_req.set_expected_session_ver(presence2->session_ver);
            deliver_client_->DeliverToUser(presence2->gateway_addr, deliver_req, deliver_resp);
        }
    }

    reactor->Finish(grpc::Status::OK);
    return reactor;
}

grpc::ServerUnaryReactor* ChatApiService::Ack(
    grpc::CallbackServerContext* context,
    const im::AckUpReq* request,
    im::AckUpResp* response) {

    auto* reactor = context->DefaultReactor();
    // M5: persist ACK to DB. For now, just acknowledge.
    response->set_ok(true);
    reactor->Finish(grpc::Status::OK);
    return reactor;
}

grpc::ServerUnaryReactor* ChatApiService::PullHistory(
    grpc::CallbackServerContext* context,
    const im::PullHistoryUpReq* request,
    im::PullHistoryUpResp* response) {

    auto* reactor = context->DefaultReactor();
    // M5: return message history from DB. Return empty for now.
    reactor->Finish(grpc::Status::OK);
    return reactor;
}
