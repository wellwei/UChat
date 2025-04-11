//
// Created by wellwei on 2025/4/7.
//

#ifndef GATESERVER_GRPCSTUBPOOL_H
#define GATESERVER_GRPCSTUBPOOL_H

#include "global.h"
#include "message.grpc.pb.h"

class GrpcStubPool {
public:
    GrpcStubPool(size_t pool_size, std::string server_address);

    ~GrpcStubPool();

    std::unique_ptr<message::VerifyService::Stub> getStub();

    void returnStub(std::unique_ptr<message::VerifyService::Stub> stub);

    void close();

private:
    std::atomic_bool _stop;
    size_t _pool_size;
    std::string _server_address;
    std::shared_ptr<grpc::Channel> _channel;
    std::queue<std::unique_ptr<message::VerifyService::Stub>> _stubs;
    std::mutex _mutex;
    std::condition_variable _cond_var;
};


#endif //GATESERVER_GRPCSTUBPOOL_H
