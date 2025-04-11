//
// Created by wellwei on 2025/4/4.
//

#ifndef GATESERVER_VERIFYGRPCCLIENT_H
#define GATESERVER_VERIFYGRPCCLIENT_H

#include "global.h"
#include "Singleton.h"
#include "GrpcStubPool.h"
#include "message.grpc.pb.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::VerifyRequest;
using message::VerifyResponse;
using message::VerifyService;

class VerifyGrpcClient : public Singleton<VerifyGrpcClient> {
    friend class Singleton<VerifyGrpcClient>;

public:
    VerifyResponse getVerifyCode(const std::string &email);

private:
    VerifyGrpcClient();

    std::unique_ptr<GrpcStubPool> stub_pool_; // gRPC 存根池
};


#endif //GATESERVER_VERIFYGRPCCLIENT_H
