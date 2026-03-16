#include "ChatTunnelService.h"
#include "ChatSession.h"
#include "RedisMgr.h"
#include "TimerWheel.h"

ChatTunnelService::ChatTunnelService(SessionRegistry* registry, const std::string& gateway_id)
    : registry_(registry), gateway_id_(gateway_id) {
}

grpc::ServerBidiReactor<im::ClientFrame, im::ServerFrame>*
ChatTunnelService::Connect(grpc::CallbackServerContext* /*context*/) {
    auto session = std::make_shared<ChatSession>(registry_, gateway_id_);
    session->SetSelf(session);
    return session.get();
}

void ChatTunnelService::StartPresenceRefreshTask() {
    TimerWheel::getInstance()->AddTask(5000, [this] {
        RefreshPresenceRoutine();
    });
}

void ChatTunnelService::RefreshPresenceRoutine() {
    auto online_sessions = registry_->GetAllSessionsPair();

    if (!online_sessions.empty()) {
        RedisMgr::getInstance()->refreshOnlineSessionsPresence(online_sessions, gateway_id_);
    }
}
