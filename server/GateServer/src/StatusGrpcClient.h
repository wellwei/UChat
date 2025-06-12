//
// Created by wellwei on 25-4-17.
//

#ifndef GATESERVER_STATUSGRPCCLIENT_H
#define GATESERVER_STATUSGRPCCLIENT_H

#include "Singleton.h"
#include "GrpcStubPool.h"
#include "message.grpc.pb.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

class StatusGrpcClient : public Singleton<StatusGrpcClient> {
    friend class Singleton<StatusGrpcClient>;

public:
    ~StatusGrpcClient();

    GetChatServerResponse getChatServer(const uint64_t &uid);

private:
    StatusGrpcClient();

    std::unique_ptr<GrpcStubPool<StatusService>> _stub;
};


#endif //GATESERVER_STATUSGRPCCLIENT_H
