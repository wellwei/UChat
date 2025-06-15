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

class VerifyGrpcClient : public Singleton<VerifyGrpcClient> {
    friend class Singleton<VerifyGrpcClient>;

public:
    Status getVerifyCode(const std::string &email) const;

private:
    VerifyGrpcClient();

    std::unique_ptr<GrpcStubPool<VerifyService>> stub_pool_; // gRPC 存根池
};


#endif //GATESERVER_VERIFYGRPCCLIENT_H
