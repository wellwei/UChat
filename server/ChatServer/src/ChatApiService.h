#pragma once

#include <grpcpp/grpcpp.h>
#include <string>
#include <memory>
#include "im.grpc.pb.h"
#include "PresenceClient.h"
#include "DeliverApiClient.h"

// Implements the ChatApi gRPC service (called by GateServer).
// Handles: SendMessage (idempotent), Ack, PullHistory (stub for M5).
class ChatApiService : public im::ChatApi::CallbackService {
public:
    ChatApiService(std::shared_ptr<PresenceClient> presence_client,
                   std::shared_ptr<DeliverApiClient> deliver_client);

    grpc::ServerUnaryReactor* SendMessage(
        grpc::CallbackServerContext* context,
        const im::SendMessageReq* request,
        im::SendMessageResp* response) override;

    grpc::ServerUnaryReactor* Ack(
        grpc::CallbackServerContext* context,
        const im::AckUpReq* request,
        im::AckUpResp* response) override;

    grpc::ServerUnaryReactor* PullHistory(
        grpc::CallbackServerContext* context,
        const im::PullHistoryUpReq* request,
        im::PullHistoryUpResp* response) override;

private:
    std::shared_ptr<PresenceClient> presence_client_;
    std::shared_ptr<DeliverApiClient> deliver_client_;
};
