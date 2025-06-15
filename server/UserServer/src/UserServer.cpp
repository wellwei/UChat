#include "UserServer.h"
#include "MysqlMgr.h"
#include "Logger.h"
#include "Auth.h"
#include "ConfigMgr.h"
#include "MQGatewayClient.h"

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
        LOG_DEBUG("创建用户成功，用户名: {}, uid: {}", request->username(), uid);
    } else {
        response->set_success(false);
        response->set_error_msg("创建用户失败，可能用户名已存在");
        LOG_ERROR("创建用户失败，用户名: {}", request->username());
    }

    response->set_code(ErrorCode::SUCCESS);

    LOG_INFO("Response CreateUser request from {}", context->peer());
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

    LOG_INFO("Response GetUserProfile request from {}", context->peer());
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
        LOG_DEBUG("密码重置成功，邮箱: {}", request->email());
    } else {
        response->set_success(false);
        response->set_error_msg("密码重置失败");
        LOG_ERROR("密码重置失败，邮箱: {}", request->email());
    }

    LOG_INFO("Response ResetPassword request from {}", context->peer());
    return grpc::Status::OK;
}   

grpc::Status UserServer::GetContacts(grpc::ServerContext* context, const GetContactsRequest* request, GetContactsResponse* response) {
    LOG_INFO("Received GetContacts request from {}", context->peer());

    if (request->uid() == 0) {
        response->set_success(false);
        response->set_error_msg("用户ID不能为空");
        return grpc::Status::OK;
    }

    const auto mysql_mgr = MysqlMgr::getInstance();
    std::vector<ContactInfo> contacts;
    if (mysql_mgr->getContacts(request->uid(), contacts)) {
        response->set_success(true);
        for (const auto& contact : contacts) {
            ContactInfo* contact_info = response->add_contacts();
            contact_info->set_uid(contact.uid());
            contact_info->set_username(contact.username());
            contact_info->set_nickname(contact.nickname());
            contact_info->set_avatar_url(contact.avatar_url());
            contact_info->set_status(contact.status());
            contact_info->set_signature(contact.signature());
        }
    } else {
        response->set_success(false);
        response->set_error_msg("获取联系人列表失败");
        LOG_ERROR("获取联系人列表失败，用户ID: {}", request->uid());
    }

    LOG_INFO("Response GetContacts request from {}", context->peer());
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
        LOG_ERROR("搜索用户失败，关键词: {}", request->keyword());
    }

    LOG_INFO("Response SearchUser request from {}", context->peer());
    return grpc::Status::OK;
}

grpc::Status UserServer::SendContactRequest(grpc::ServerContext* context, const SendContactRequestArgs* request, SendContactRequestReply* response) {
    LOG_INFO("Received SendContactRequest request from {}", context->peer());

    if (request->requester_id() == 0 || request->addressee_id() == 0) {
        response->set_success(false);
        response->set_error_msg("用户ID不能为空");
        return grpc::Status::OK;
    }

    if (request->requester_id() == request->addressee_id()) {
        response->set_success(false);
        response->set_error_msg("不能添加自己为联系人");
        return grpc::Status::OK;
    }

    const auto mysql_mgr = MysqlMgr::getInstance();
    uint64_t request_id = 0;
    if (mysql_mgr->sendContactRequest(request->requester_id(), request->addressee_id(), request->request_message(), request_id)) {
        response->set_request_id(request_id);
        LOG_DEBUG("发送联系人请求成功，请求ID: {}", request_id);
        
        // 发送MQ通知
        auto mqClient = MQGatewayClient::getInstance();
        if (mqClient->isConnected()) {
            // 获取请求者信息
            UserProfile requesterProfile;
            if (mysql_mgr->getUserProfile(request->requester_id(), requesterProfile)) {
                std::string requesterName = requesterProfile.nickname().empty() ? 
                                          requesterProfile.username() : requesterProfile.nickname();
                
                mqClient->publishContactRequestNotification(
                    request->addressee_id(),
                    request->requester_id(),
                    request_id,
                    requesterName,
                    request->request_message()
                );
                response->set_success(true);
                LOG_DEBUG("联系人请求通知已发送到MQ");
            }
        } else {
            LOG_CRITICAL("MQGateway未连接，无法发送通知");
            response->set_success(false);
        }
    } else {
        response->set_success(false);
        response->set_error_msg("发送联系人请求失败，可能已经是联系人或有待处理的请求");
        LOG_ERROR("发送联系人请求失败，请求者ID: {}, 接收者ID: {}", request->requester_id(), request->addressee_id());
    }

    LOG_INFO("Response SendContactRequest request from {}", context->peer());
    return grpc::Status::OK;
}

grpc::Status UserServer::HandleContactRequest(grpc::ServerContext* context, const HandleContactRequestArgs* request, HandleContactRequestReply* response) {
    LOG_INFO("Received HandleContactRequest request from {}", context->peer());

    if (request->request_id() == 0 || request->user_id() == 0) {
        response->set_success(false);
        response->set_error_msg("请求ID或用户ID不能为空");
        return grpc::Status::OK;
    }

    const auto mysql_mgr = MysqlMgr::getInstance();
    
    // 先获取请求详情，以便发送通知
    ContactRequestInfo requestInfo;
    if (!mysql_mgr->getContactRequestById(request->request_id(), requestInfo)) {
        response->set_success(false);
        response->set_error_msg("联系人请求不存在");
        return grpc::Status::OK;
    }
    
    // 验证请求是否属于当前用户
    if (requestInfo.requester_id() != request->user_id() && 
        requestInfo.addressee_id() != request->user_id()) {
        response->set_success(false);
        response->set_error_msg("无权处理此联系人请求");
        return grpc::Status::OK;
    }
    
    if (mysql_mgr->handleContactRequest(request->request_id(), request->user_id(), request->action())) {
        response->set_success(true);
        LOG_DEBUG("处理联系人请求成功，请求ID: {}, 用户ID: {}, 动作: {}", 
                 request->request_id(), request->user_id(), 
                 request->action() == ContactRequestStatus::ACCEPTED ? "接受" : "拒绝");
        
        // 发送MQ通知给请求者
        auto mqClient = MQGatewayClient::getInstance();
        if (mqClient->isConnected()) {
            // 获取处理者信息
            UserProfile handlerProfile;
            if (mysql_mgr->getUserProfile(request->user_id(), handlerProfile)) {
                std::string handlerName = handlerProfile.nickname().empty() ? 
                                        handlerProfile.username() : handlerProfile.nickname();
                
                // 发送通知给请求者
                uint64_t targetUserId = requestInfo.requester_id();
                if (request->action() == ContactRequestStatus::ACCEPTED) {
                    mqClient->publishContactAcceptedNotification(
                        targetUserId,
                        request->user_id(),
                        handlerName
                    );
                    LOG_DEBUG("联系人接受通知已发送给用户 {}", targetUserId);
                } else if (request->action() == ContactRequestStatus::REJECTED) {
                    mqClient->publishContactRejectedNotification(
                        targetUserId,
                        request->user_id(),
                        handlerName
                    );
                    LOG_DEBUG("联系人拒绝通知已发送给用户 {}", targetUserId);
                }
            }
        } else {
            LOG_ERROR("MQGateway未连接，无法发送通知");
            response->set_success(false);
        }
    } else {
        response->set_success(false);
        response->set_error_msg("处理联系人请求失败，可能请求不存在或已被处理");
        LOG_ERROR("处理联系人请求失败，请求ID: {}, 用户ID: {}", request->request_id(), request->user_id());
    }

    LOG_INFO("Response HandleContactRequest request from {}", context->peer());
    return grpc::Status::OK;
}

grpc::Status UserServer::GetContactRequests(grpc::ServerContext* context, const GetContactRequestsArgs* request, GetContactRequestsReply* response) {
    LOG_INFO("Received GetContactRequests request from {}", context->peer());

    if (request->user_id() == 0) {
        response->set_success(false);
        response->set_error_msg("用户ID不能为空");
        return grpc::Status::OK;
    }

    const auto mysql_mgr = MysqlMgr::getInstance();
    std::vector<ContactRequestInfo> requests;
    if (mysql_mgr->getContactRequests(request->user_id(), request->status(), requests)) {
        response->set_success(true);
        for (const auto& request_info : requests) {
            response->add_requests()->CopyFrom(request_info);
        }
        LOG_DEBUG("获取联系人请求列表成功，用户ID: {}, 状态: {}, 数量: {}", 
                std::to_string( request->user_id()), std::to_string(request->status()), std::to_string(requests.size()));
    } else {
        response->set_success(false);
        response->set_error_msg("获取联系人请求列表失败");
        LOG_ERROR("获取联系人请求列表失败，用户ID: {}", request->user_id());
    }

    LOG_INFO("Response GetContactRequests request from {}", context->peer());
    return grpc::Status::OK;
}

void UserServer::start() {
    auto config = *ConfigMgr::getInstance();
    std::string server_address = config["UserServer"]["host"] + ":" + config["UserServer"]["port"];
    
    // 初始化MQGateway客户端
    auto mqClient = MQGatewayClient::getInstance();
    std::string mqGatewayAddress = config["MQGateway"]["host"] + ":" + config["MQGateway"]["port"];
    if (!mqClient->initialize(mqGatewayAddress)) {
        LOG_WARN("初始化MQGateway客户端失败，将在没有消息通知的情况下运行");
    }
    
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
    
    // 关闭MQGateway客户端
    auto mqClient = MQGatewayClient::getInstance();
    mqClient->shutdown();
    
    if (server_) {
        server_->Shutdown();
    }
    LOG_INFO("UserServer 已关闭");
}