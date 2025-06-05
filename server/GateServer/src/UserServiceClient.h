#pragma once 

#include "user_service.grpc.pb.h"
#include "user_service.pb.h"
#include "Singleton.h"
#include "GrpcStubPool.h"
#include <nlohmann/json.hpp>

using grpc::ClientContext;
using grpc::Status;

class UserServiceClient : public Singleton<UserServiceClient> {
    friend class Singleton<UserServiceClient>;

public:
    ~UserServiceClient();

    // 验证用户凭证
    bool VerifyCredentials(const std::string& handle, const std::string& password, nlohmann::json& response);

    // 创建用户
    bool CreateUser(const std::string& username, const std::string& password, const std::string& email, uint64_t& uid);

    // 获取用户资料
    bool GetUserProfile(const uint64_t uid, nlohmann::json& user_info);
    
    // 更新用户资料
    bool UpdateUserProfile(const uint64_t uid, const nlohmann::json& user_info);

    // 重置密码
    bool ResetPassword(const std::string& email, const std::string& new_password);

private:
    UserServiceClient();
    
    std::unique_ptr<GrpcStubPool<UserService>> _stub;
};
