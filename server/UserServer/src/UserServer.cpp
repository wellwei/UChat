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
    if (UserProfile user_profile; mysql_mgr->verifyCredentials(request->handle(), request->password(), uid, user_profile)) {
        response->set_success(true);
        response->set_uid(uid);
        
        // 使用mutable_user_profile正确设置用户信息
        UserProfile* profile = response->mutable_user_profile();
        *profile = user_profile;
        auto auth = Auth::getInstance();
        response->set_token(auth->generateToken(uid));

    } else {
        response->set_success(false);
        response->set_error_msg("用户名/邮箱或密码错误");
    }
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
    if (uint64_t uid = 0; mysql_mgr->createUser(request->username(), request->password(), request->email(), uid)) {
        response->set_success(true);
        response->set_uid(uid);
        LOG_DEBUG("创建用户成功，用户名: {}, uid: {}", request->username(), uid);
    } else {
        response->set_success(false);
        response->set_error_msg("创建用户失败，可能用户名已存在");
        LOG_ERROR("创建用户失败，用户名: {}", request->username());
    }
    
    return grpc::Status::OK;
}

grpc::Status UserServer::GetUserProfile(grpc::ServerContext* context, const GetUserProfileRequest* request, GetUserProfileResponse* response) {
    LOG_INFO("Received GetUserProfile request from {}", context->peer());
    if (request->uid() == 0) {
        response->set_success(false);
        response->set_error_msg("用户ID错误");
        return grpc::Status::OK;
    }

    auto mysql_mgr = MysqlMgr::getInstance();
    UserProfile user_profile;
    if (mysql_mgr->getUserProfile(request->uid(), user_profile)) {
        response->set_success(true);
        
        // 使用mutable_user_profile正确设置用户信息
        UserProfile* profile = response->mutable_user_profile();
        *profile = user_profile;
    } else {
        response->set_success(false);
        response->set_error_msg("获取用户信息失败");
    }
    return grpc::Status::OK;
}

grpc::Status UserServer::UpdateUserProfile(grpc::ServerContext* context, const UpdateUserProfileRequest* request, UpdateUserProfileResponse* response) {
    LOG_INFO("Received UpdateUserProfile request from {}", context->peer());
    if (request->uid() == 0) {
        response->set_success(false);
        response->set_error_msg("用户ID错误");
        return grpc::Status::OK;
    }

    auto mysql_mgr = MysqlMgr::getInstance();
    // 检查用户是否存在
    UserProfile user_profile;
    if (!mysql_mgr->getUserProfile(request->uid(), user_profile)) {
        response->set_success(false);
        response->set_error_msg("用户不存在");
        return grpc::Status::OK;
    }
    
    if (mysql_mgr->updateUserProfile(request->uid(), request->user_profile())) {
        response->set_success(true);
        // 更新用户信息
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
        LOG_DEBUG("密码重置成功，邮箱: {}", request->email());
    } else {
        response->set_success(false);
        response->set_error_msg("密码重置失败");
        LOG_ERROR("密码重置失败，邮箱: {}", request->email());
    }
    return grpc::Status::OK;
}   

void UserServer::start() {
    auto config = *ConfigMgr::getInstance();
    std::string server_address = config["UserServer"]["host"] + ":" + config["UserServer"]["port"];
    
    grpc::ServerBuilder builder;
    
    // 设置服务器地址，使用不安全的通道（没有SSL认证）
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    
    // 注册服务
    builder.RegisterService(this);
    
    // 设置最大消息大小（可选）
    builder.SetMaxReceiveMessageSize(10 * 1024 * 1024); // 10MB
    
    // 构建并启动服务器
    server_ = builder.BuildAndStart();
    
    LOG_INFO("UserServer 启动成功，监听地址: {}", server_address);
    server_->Wait();
}

void UserServer::stop() {
    LOG_INFO("UserServer 开始关闭...");
    if (server_) {
        server_->Shutdown();
    }
    LOG_INFO("UserServer 已关闭");
}