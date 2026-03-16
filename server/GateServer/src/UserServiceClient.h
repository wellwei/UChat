#pragma once

#include "userserver.grpc.pb.h"
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
    uint32_t CreateUser(const std::string& username, const std::string& password, const std::string& email, uint64_t& uid);

    // 获取用户资料
    bool GetUserProfile(const uint64_t uid, nlohmann::json& user_info);

    // 更新用户资料 (返回更新后的完整 Profile)
    bool UpdateUserProfile(const uint64_t uid, const nlohmann::json& user_info, nlohmann::json& updated_profile);

    // 重置密码
    bool ResetPassword(const std::string& email, const std::string& new_password);

    // 搜索用户
    bool SearchUser(const std::string& keyword, nlohmann::json& users);

    // M3: 联系人操作
    bool SendContactRequest(uint64_t from_uid, uint64_t to_uid, const std::string& note, nlohmann::json& out_json);
    bool HandleContactRequest(uint64_t request_id, uint64_t handler_uid, bool accept, nlohmann::json& out_json);
    bool GetContacts(uint64_t uid, nlohmann::json& out_json);
    bool GetContactRequests(uint64_t uid, nlohmann::json& out_json);

private:
    UserServiceClient();

    std::unique_ptr<GrpcStubPool<UserService>> _stub;
};
