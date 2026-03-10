#pragma once

#include <grpcpp/grpcpp.h>
#include <string>
#include <cstdint>
#include "im.grpc.pb.h"
#include "SessionRegistry.h"

// Implements the DeliverApi service (called by ChatServer to push messages
// to users connected to this GateServer instance).
class DeliverApiService : public im::DeliverApi::CallbackService {
public:
    explicit DeliverApiService(SessionRegistry* registry);

    grpc::ServerUnaryReactor* DeliverToUser(
        grpc::CallbackServerContext* context,
        const im::DeliverToUserReq* request,
        im::DeliverToUserResp* response) override;

    grpc::ServerUnaryReactor* DeliverBatch(
        grpc::CallbackServerContext* context,
        const im::DeliverBatchReq* request,
        im::DeliverBatchResp* response) override;

private:
    SessionRegistry* registry_;
};
