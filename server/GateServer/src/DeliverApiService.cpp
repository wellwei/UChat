#include "DeliverApiService.h"
#include "ChatSession.h"
#include "Logger.h"

DeliverApiService::DeliverApiService(SessionRegistry* registry)
    : registry_(registry) {
}

grpc::ServerUnaryReactor* DeliverApiService::DeliverToUser(
    grpc::CallbackServerContext* context,
    const im::DeliverToUserReq* request,
    im::DeliverToUserResp* response) {

    auto* reactor = context->DefaultReactor();

    const auto session = registry_->Find(request->to_uid());
    if (!session) {
        response->set_ok(false);
        response->set_online(false);
        response->set_reason("User not on this gateway");
        reactor->Finish(grpc::Status::OK);
        return reactor;
    }

    // Optional session_ver check: if expected_session_ver != 0, validate
    if (request->expected_session_ver() != 0 &&
        session->GetSessionVer() != request->expected_session_ver()) {
        response->set_ok(false);
        response->set_online(false);
        response->set_reason("Session version mismatch");
        reactor->Finish(grpc::Status::OK);
        return reactor;
    }

    im::ServerFrame frame;
    *frame.mutable_push()->mutable_env() = request->env();
    session->Enqueue(std::move(frame));

    response->set_ok(true);
    response->set_online(true);
    reactor->Finish(grpc::Status::OK);
    return reactor;
}

grpc::ServerUnaryReactor* DeliverApiService::DeliverBatch(
    grpc::CallbackServerContext* context,
    const im::DeliverBatchReq* request,
    im::DeliverBatchResp* response) {

    auto* reactor = context->DefaultReactor();

    int32_t delivered = 0;
    int32_t offline = 0;

    for (int64_t uid : request->to_uids()) {
        const auto session = registry_->Find(uid);
        if (!session) {
            ++offline;
            continue;
        }
        im::ServerFrame frame;
        *frame.mutable_push()->mutable_env() = request->env();
        session->Enqueue(std::move(frame));
        ++delivered;
    }

    response->set_delivered(delivered);
    response->set_offline(offline);
    reactor->Finish(grpc::Status::OK);
    return reactor;
}
