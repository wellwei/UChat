#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include "Singleton.h"
#include "statusserver.grpc.pb.h"

using grpc::Status;
using grpc::ClientContext;
using grpc::Channel;

class StatusGrpcClient : public Singleton<StatusGrpcClient> {
    friend class Singleton<StatusGrpcClient>;

public:
    // 查询用户所在的 ChatServer
    GetUidChatServerResponse GetUidChatServer(const uint64_t &uid);
    
    // 健康检查
    bool isConnected();

private:
    StatusGrpcClient();
    
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<StatusService::Stub> stub_;
    std::string server_address_;
}; 