#pragma once

#include <grpcpp/grpcpp.h>
#include <string>
#include "im.grpc.pb.h"
#include "SessionRegistry.h"

// Factory service for ChatTunnel.Connect RPC.
// Creates a new ChatSession for each connecting client.
class ChatTunnelService : public im::ChatTunnel::CallbackService {
public:
    ChatTunnelService(SessionRegistry* registry, const std::string& gateway_id);

    grpc::ServerBidiReactor<im::ClientFrame, im::ServerFrame>*
    Connect(grpc::CallbackServerContext* context) override;

    void StartPresenceRefreshTask();

private:
    void RefreshPresenceRoutine();
    SessionRegistry* registry_;
    std::string gateway_id_;
};
