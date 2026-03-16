#pragma once

#include <grpcpp/grpcpp.h>
#include <string>
#include <memory>
#include "im.grpc.pb.h"
#include "DeliverApiClient.h"

// Implements the ChatApi gRPC service (called by GateServer).
// Handles: SendMessage (idempotent, fire-and-forget), Ack, Sync.
// Uses RedisMgr singleton directly for all Redis operations.
class ChatApiService : public im::ChatApi::CallbackService {
public:
    explicit ChatApiService(std::shared_ptr<DeliverApiClient> deliver_client);

    grpc::ServerUnaryReactor* SendMessage(
        grpc::CallbackServerContext* context,
        const im::SendMessageReq* request,
        im::SendMessageResp* response) override;

    grpc::ServerUnaryReactor* Ack(
        grpc::CallbackServerContext* context,
        const im::AckUpReq* request,
        im::AckUpResp* response) override;

    // Sync: pull missed messages based on client's MaxSeqID per conversation
    grpc::ServerUnaryReactor* Sync(
        grpc::CallbackServerContext* context,
        const im::SyncUpReq* request,
        im::SyncUpResp* response) override;

private:
    std::shared_ptr<DeliverApiClient> deliver_client_;
};
