#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include "userserver.grpc.pb.h"

class GrpcClient {
public:
    GrpcClient(const std::string& target);
    ~GrpcClient();

    // 测试 GetUserProfile RPC
    bool GetUserProfile(uint64_t uid);

    // 测试 SearchUser RPC
    bool SearchUser(const std::string& keyword);

private:
    std::unique_ptr<UserService::Stub> stub_;
    std::string target_;
};
