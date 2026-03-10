#include "UserServer.h"
#include "MysqlMgr.h"
#include "Logger.h"
#include "Auth.h"
#include "ConfigMgr.h"

grpc::Status UserServer::VerifyCredentials(grpc::ServerContext* context, const VerifyCredentialsRequest* request, VerifyCredentialsResponse* response) {
    LOG_INFO("Received VerifyCredentials request from {}", context->peer());

    if (request->handle().empty() || request->password().empty()) {
        response->set_success(false);
        response->set_error_msg("用户名/邮箱或密码不能为空");
        return grpc::Status::OK;
    }

    const auto mysql_mgr = MysqlMgr::getInstance();
    uint64_t uid = 0;
    if (mysql_mgr->verifyCredentials(request->handle(), request->password(), uid)) {
        response->set_success(true);
        response->set_uid(uid);
        auto auth = Auth::getInstance();
        response->set_token(auth->generateToken(uid));

    } else {
        response->set_success(false);
        response->set_error_msg("用户名/邮箱或密码错误");
    }
    LOG_INFO("Response VerifyCredentials request from {}", context->peer());
    return grpc::Status::OK;
}

grpc::Status UserServer::CreateUser(grpc::ServerContext* context, const CreateUserRequest* request, CreateUserResponse* response) {
    LOG_INFO("Received CreateUser request from {}", context->peer());
    if (request->username().empty() || request->password().empty()) {
        response->set_success(false);
        response->set_error_msg("用户名或密码不能为空");
        return grpc::Status::OK;
    }

    const auto mysql_mgr = MysqlMgr::getInstance();

    if (mysql_mgr->exists_user(request->username(), request->email())) {
        response->set_success(false);
        response->set_code(ErrorCode::USER_EXISTS);
        response->set_error_msg("用户名或邮箱已存在");
        return grpc::Status::OK;
    }

    uint64_t uid = 0;
    if (mysql_mgr->createUser(request->username(), request->password(), request->email(), uid)) {
        response->set_success(true);
        response->set_uid(uid);
        LOG_DEBUG("创建用户成功，用户名：{}, uid: {}", request->username(), uid);
    } else {
        response->set_success(false);
        response->set_error_msg("创建用户失败，可能用户名已存在");
        LOG_ERROR("创建用户失败，用户名：{}", request->username());
    }

    response->set_code(ErrorCode::SUCCESS);

    LOG_INFO("Response CreateUser request from {}", context->peer());
    return grpc::Status::OK;
}

grpc::Status UserServer::GetUserProfile(grpc::ServerContext* context, const GetUserProfileRequest* request, GetUserProfileResponse* response) {
    LOG_INFO("Received GetUserProfile request from {}", context->peer());
    if (request->uid() == 0) {
        response->set_success(false);
        response->set_error_msg("用户 ID 错误");
        return grpc::Status::OK;
    }

    auto mysql_mgr = MysqlMgr::getInstance();
    UserProfile user_profile;
    if (mysql_mgr->getUserProfile(request->uid(), user_profile)) {
        response->set_success(true);

        UserProfile* profile = response->mutable_user_profile();
        *profile = user_profile;
    } else {
        response->set_success(false);
        response->set_error_msg("获取用户信息失败");
    }

    LOG_INFO("Response GetUserProfile request from {}", context->peer());
    return grpc::Status::OK;
}

grpc::Status UserServer::UpdateUserProfile(grpc::ServerContext* context, const UpdateUserProfileRequest* request, UpdateUserProfileResponse* response) {
    LOG_INFO("Received UpdateUserProfile request from {}", context->peer());
    if (request->uid() == 0) {
        response->set_success(false);
        response->set_error_msg("用户 ID 错误");
        return grpc::Status::OK;
    }

    auto mysql_mgr = MysqlMgr::getInstance();
    UserProfile user_profile;
    if (!mysql_mgr->getUserProfile(request->uid(), user_profile)) {
        response->set_success(false);
        response->set_error_msg("用户不存在");
        return grpc::Status::OK;
    }

    if (mysql_mgr->updateUserProfile(request->uid(), request->user_profile())) {
        response->set_success(true);
        if (mysql_mgr->getUserProfile(request->uid(), user_profile)) {
            response->mutable_user_profile()->CopyFrom(user_profile);
        } else {
            response->set_success(false);
            response->set_error_msg("获取更新后的用户信息失败");
        }
    } else {
        response->set_success(false);
        response->set_error_msg("更新用户信息失败");
    }

    LOG_INFO("Response UpdateUserProfile request from {}", context->peer());
    return grpc::Status::OK;
}

grpc::Status UserServer::ResetPassword(grpc::ServerContext* context, const ResetPasswordRequest* request, ResetPasswordResponse* response) {
    LOG_INFO("Received ResetPassword request from {}", context->peer());
    if (request->email().empty() || request->new_password().empty()) {
        response->set_success(false);
        response->set_error_msg("邮箱或新密码不能为空");
        return grpc::Status::OK;
    }

    const auto mysql_mgr = MysqlMgr::getInstance();
    if (mysql_mgr->resetPassword(request->email(), request->new_password())) {
        response->set_success(true);
        LOG_DEBUG("密码重置成功，邮箱：{}", request->email());
    } else {
        response->set_success(false);
        response->set_error_msg("密码重置失败");
        LOG_ERROR("密码重置失败，邮箱：{}", request->email());
    }

    LOG_INFO("Response ResetPassword request from {}", context->peer());
    return grpc::Status::OK;
}

grpc::Status UserServer::SearchUser(grpc::ServerContext* context, const SearchUserRequest* request, SearchUserResponse* response) {
    LOG_INFO("Received SearchUser request from {}", context->peer());

    if (request->keyword().empty()) {
        response->set_success(false);
        response->set_error_msg("关键词不能为空");
        return grpc::Status::OK;
    }
    const auto mysql_mgr = MysqlMgr::getInstance();
    std::vector<UserProfile> users;
    if (mysql_mgr->searchUser(request->keyword(), users)) {
        response->set_success(true);
        for (const auto& user : users) {
            UserProfile* user_profile = response->add_users();
            user_profile->set_uid(user.uid());
            user_profile->set_username(user.username());
            user_profile->set_nickname(user.nickname());
            user_profile->set_avatar_url(user.avatar_url());
            user_profile->set_status(user.status());
            user_profile->set_signature(user.signature());
        }
        LOG_DEBUG("搜索 {} 成功", request->keyword());
    } else {
        response->set_success(false);
        response->set_error_msg("搜索用户失败");
        LOG_ERROR("搜索用户失败，关键词：{}", request->keyword());
    }

    LOG_INFO("Response SearchUser request from {}", context->peer());
    return grpc::Status::OK;
}

void UserServer::start() {
    auto config = *ConfigMgr::getInstance();
    std::string server_address = config["UserServer"]["host"] + ":" + config["UserServer"]["port"];

    grpc::ServerBuilder builder;

    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    builder.RegisterService(this);

    builder.SetMaxReceiveMessageSize(10 * 1024 * 1024);

    server_ = builder.BuildAndStart();

    LOG_INFO("UserServer 启动成功，监听地址：{}", server_address);
    server_->Wait();
}

void UserServer::stop() {
    LOG_INFO("UserServer 开始关闭...");

    if (server_) {
        server_->Shutdown();
    }
    LOG_INFO("UserServer 已关闭");
}
