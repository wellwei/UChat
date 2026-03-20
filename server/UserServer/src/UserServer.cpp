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

// ========== M3: 联系人操作 ==========

grpc::Status UserServer::SendContactRequest(grpc::ServerContext* context,
                                            const SendContactRequestReq* request,
                                            SendContactRequestResp* response) {
    LOG_INFO("Received SendContactRequest from_uid={} to_uid={}", request->from_uid(), request->to_uid());

    if (request->from_uid() == 0 || request->to_uid() == 0) {
        response->set_success(false);
        response->set_error_msg("from_uid 和 to_uid 不能为 0");
        return grpc::Status::OK;
    }
    if (request->from_uid() == request->to_uid()) {
        response->set_success(false);
        response->set_error_msg("不能添加自己为联系人");
        return grpc::Status::OK;
    }

    auto mysql_mgr = MysqlMgr::getInstance();
    uint64_t request_id = 0;
    if (mysql_mgr->sendContactRequest(request->from_uid(), request->to_uid(),
                                      request->note(), request_id)) {
        response->set_success(true);
        response->set_request_id(request_id);
    } else {
        response->set_success(false);
        response->set_error_msg("发送好友申请失败（已是好友或其他错误）");
    }

    LOG_INFO("Response SendContactRequest from_uid={} to_uid={} success={}",
             request->from_uid(), request->to_uid(), response->success());
    return grpc::Status::OK;
}

grpc::Status UserServer::HandleContactRequest(grpc::ServerContext* context,
                                              const HandleContactRequestReq* request,
                                              HandleContactRequestResp* response) {
    LOG_INFO("Received HandleContactRequest request_id={} handler_uid={} accept={}",
             request->request_id(), request->handler_uid(), request->accept());

    if (request->request_id() == 0 || request->handler_uid() == 0) {
        response->set_success(false);
        response->set_error_msg("参数无效");
        return grpc::Status::OK;
    }

    auto mysql_mgr = MysqlMgr::getInstance();
    uint64_t from_uid = 0;
    if (mysql_mgr->handleContactRequest(request->request_id(), request->handler_uid(),
                                        request->accept(), from_uid)) {
        response->set_success(true);
        response->set_from_uid(from_uid);
    } else {
        response->set_success(false);
        response->set_error_msg("处理好友申请失败（请求不存在或无权限）");
    }

    LOG_INFO("Response HandleContactRequest request_id={} success={} from_uid={}",
             request->request_id(), response->success(), response->from_uid());
    return grpc::Status::OK;
}

grpc::Status UserServer::GetContacts(grpc::ServerContext* context,
                                     const GetContactsReq* request,
                                     GetContactsResp* response) {
    LOG_INFO("Received GetContacts uid={}", request->uid());

    if (request->uid() == 0) {
        response->set_success(false);
        response->set_error_msg("uid 不能为 0");
        return grpc::Status::OK;
    }

    auto mysql_mgr = MysqlMgr::getInstance();
    std::vector<ContactEntry> contacts;
    if (mysql_mgr->getContacts(request->uid(), contacts)) {
        response->set_success(true);
        for (const auto& entry : contacts) {
            auto* e = response->add_contacts();
            e->set_uid(entry.uid());
            e->set_username(entry.username());
            e->set_nickname(entry.nickname());
            e->set_avatar_url(entry.avatar_url());
        }
    } else {
        response->set_success(false);
        response->set_error_msg("获取联系人列表失败");
    }

    LOG_INFO("Response GetContacts uid={} count={}", request->uid(), response->contacts_size());
    return grpc::Status::OK;
}

grpc::Status UserServer::GetContactRequests(grpc::ServerContext* context,
                                            const GetContactRequestsReq* request,
                                            GetContactRequestsResp* response) {
    LOG_INFO("Received GetContactRequests uid={}", request->uid());

    if (request->uid() == 0) {
        response->set_success(false);
        response->set_error_msg("uid 不能为 0");
        return grpc::Status::OK;
    }

    auto mysql_mgr = MysqlMgr::getInstance();
    std::vector<ContactRequest> contact_requests;
    if (mysql_mgr->getContactRequests(request->uid(), contact_requests)) {
        response->set_success(true);
        for (const auto& req : contact_requests) {
            auto* r = response->add_requests();
            r->set_request_id(req.request_id());
            r->set_from_uid(req.from_uid());
            r->set_from_username(req.from_username());
            r->set_from_avatar(req.from_avatar());
            r->set_note(req.note());
            r->set_ts_ms(req.ts_ms());
        }
    } else {
        response->set_success(false);
        response->set_error_msg("获取好友申请列表失败");
    }

    LOG_INFO("Response GetContactRequests uid={} count={}", request->uid(), response->requests_size());
    return grpc::Status::OK;
}

// ========== M4: 群聊操作 ==========

grpc::Status UserServer::CreateGroup(grpc::ServerContext* context,
                                     const CreateGroupReq* request,
                                     CreateGroupResp* response) {
    LOG_INFO("Received CreateGroup owner_uid={} name={}", request->owner_uid(), request->name());

    if (request->owner_uid() == 0 || request->name().empty()) {
        response->set_success(false);
        response->set_error_msg("owner_uid 或群名称无效");
        return grpc::Status::OK;
    }

    GroupInfo group;
    if (MysqlMgr::getInstance()->createGroup(*request, group)) {
        response->set_success(true);
        *response->mutable_group() = group;
    } else {
        response->set_success(false);
        response->set_error_msg("创建群聊失败");
    }
    return grpc::Status::OK;
}

grpc::Status UserServer::UpdateGroup(grpc::ServerContext* context,
                                     const UpdateGroupReq* request,
                                     UpdateGroupResp* response) {
    LOG_INFO("Received UpdateGroup group_id={} operator_uid={}", request->group_id(), request->operator_uid());

    GroupInfo group;
    if (MysqlMgr::getInstance()->updateGroup(*request, group)) {
        response->set_success(true);
        *response->mutable_group() = group;
    } else {
        response->set_success(false);
        response->set_error_msg("更新群聊失败");
    }
    return grpc::Status::OK;
}

grpc::Status UserServer::DeleteGroup(grpc::ServerContext* context,
                                     const DeleteGroupReq* request,
                                     DeleteGroupResp* response) {
    LOG_INFO("Received DeleteGroup group_id={} operator_uid={}", request->group_id(), request->operator_uid());

    response->set_success(MysqlMgr::getInstance()->deleteGroup(request->operator_uid(), request->group_id()));
    if (!response->success()) {
        response->set_error_msg("删除群聊失败");
    }
    return grpc::Status::OK;
}

grpc::Status UserServer::GetGroup(grpc::ServerContext* context,
                                  const GetGroupReq* request,
                                  GetGroupResp* response) {
    LOG_INFO("Received GetGroup group_id={} requester_uid={}", request->group_id(), request->requester_uid());

    GroupInfo group;
    if (MysqlMgr::getInstance()->getGroup(request->requester_uid(), request->group_id(), group)) {
        response->set_success(true);
        *response->mutable_group() = group;
    } else {
        response->set_success(false);
        response->set_error_msg("群聊不存在");
    }
    return grpc::Status::OK;
}

grpc::Status UserServer::SearchGroups(grpc::ServerContext* context,
                                      const SearchGroupsReq* request,
                                      SearchGroupsResp* response) {
    LOG_INFO("Received SearchGroups keyword={}", request->keyword());

    if (request->keyword().empty()) {
        response->set_success(false);
        response->set_error_msg("关键词不能为空");
        return grpc::Status::OK;
    }

    std::vector<GroupInfo> groups;
    if (MysqlMgr::getInstance()->searchGroups(request->keyword(), request->limit(), groups)) {
        response->set_success(true);
        for (const auto& group : groups) {
            *response->add_groups() = group;
        }
    } else {
        response->set_success(false);
        response->set_error_msg("搜索群聊失败");
    }
    return grpc::Status::OK;
}

grpc::Status UserServer::ListMyGroups(grpc::ServerContext* context,
                                      const ListMyGroupsReq* request,
                                      ListMyGroupsResp* response) {
    LOG_INFO("Received ListMyGroups uid={}", request->uid());

    if (request->uid() == 0) {
        response->set_success(false);
        response->set_error_msg("uid 无效");
        return grpc::Status::OK;
    }

    std::vector<GroupInfo> groups;
    if (MysqlMgr::getInstance()->listMyGroups(request->uid(), groups)) {
        response->set_success(true);
        for (const auto& group : groups) {
            *response->add_groups() = group;
        }
    } else {
        response->set_success(false);
        response->set_error_msg("获取群列表失败");
    }
    return grpc::Status::OK;
}

grpc::Status UserServer::JoinGroup(grpc::ServerContext* context,
                                   const JoinGroupReq* request,
                                   JoinGroupResp* response) {
    LOG_INFO("Received JoinGroup uid={} group_id={}", request->uid(), request->group_id());

    response->set_success(MysqlMgr::getInstance()->joinGroup(request->uid(), request->group_id()));
    if (!response->success()) {
        response->set_error_msg("加入群聊失败");
    }
    return grpc::Status::OK;
}

grpc::Status UserServer::QuitGroup(grpc::ServerContext* context,
                                   const QuitGroupReq* request,
                                   QuitGroupResp* response) {
    LOG_INFO("Received QuitGroup uid={} group_id={}", request->uid(), request->group_id());

    response->set_success(MysqlMgr::getInstance()->quitGroup(request->uid(), request->group_id()));
    if (!response->success()) {
        response->set_error_msg("退出群聊失败");
    }
    return grpc::Status::OK;
}

grpc::Status UserServer::GetGroupMembers(grpc::ServerContext* context,
                                         const GetGroupMembersReq* request,
                                         GetGroupMembersResp* response) {
    LOG_INFO("Received GetGroupMembers requester_uid={} group_id={}", request->requester_uid(), request->group_id());

    std::vector<GroupMemberInfo> members;
    if (MysqlMgr::getInstance()->getGroupMembers(request->requester_uid(), request->group_id(), members)) {
        response->set_success(true);
        for (const auto& member : members) {
            *response->add_members() = member;
        }
    } else {
        response->set_success(false);
        response->set_error_msg("获取群成员失败");
    }
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
