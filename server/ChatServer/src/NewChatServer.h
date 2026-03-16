#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include "im.grpc.pb.h"
#include "ChatApiService.h"
#include "DeliverApiClient.h"

// Replacement ChatServer entry point.
// Registers ChatApiService (gRPC Callback API) on the configured port.
// ChatServer no longer holds client connections - GateServer manages those.
class NewChatServer {
public:
    NewChatServer();
    ~NewChatServer();

    // Starts the gRPC server; blocks until Stop() is called or server fails.
    void Start();

    void Stop();

private:
    std::string host_;
    std::string grpc_port_;

    std::shared_ptr<DeliverApiClient> deliver_client_;
    std::unique_ptr<ChatApiService> chat_api_service_;

    std::unique_ptr<grpc::Server> grpc_server_;
};
