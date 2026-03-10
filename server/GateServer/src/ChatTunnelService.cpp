#include "ChatTunnelService.h"
#include "ChatSession.h"

ChatTunnelService::ChatTunnelService(SessionRegistry* registry, const std::string& gateway_id, int online_ttl_seconds)
    : registry_(registry), gateway_id_(gateway_id), online_ttl_seconds_(online_ttl_seconds) {
}

grpc::ServerBidiReactor<im::ClientFrame, im::ServerFrame>*
ChatTunnelService::Connect(grpc::CallbackServerContext* /*context*/) {
    auto session = std::make_shared<ChatSession>(registry_, gateway_id_, online_ttl_seconds_);
    session->SetSelf(session);
    return session.get();
}
