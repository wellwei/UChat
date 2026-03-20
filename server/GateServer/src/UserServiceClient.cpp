#include "UserServiceClient.h"
#include "Logger.h"
#include "ConfigMgr.h"

UserServiceClient::UserServiceClient() {
    auto config = *ConfigMgr::getInstance();
    std::string server_address = config["UserServer"]["host"] + ":" + config["UserServer"]["port"];
    _stub = std::make_unique<GrpcStubPool<UserService>>(8, server_address);
}

UserServiceClient::~UserServiceClient() {
    _stub->close();
}

bool UserServiceClient::VerifyCredentials(const std::string& handle, const std::string& password, nlohmann::json& response) {
    VerifyCredentialsRequest request;
    VerifyCredentialsResponse reply;
    ClientContext context;

    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    request.set_handle(handle);
    request.set_password(password);

    auto stub = _stub->getStub();
    const Status status = stub->VerifyCredentials(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (status.ok()) {
        if (reply.success()) {
            response["uid"] = reply.uid();
            response["token"] = reply.token();
            return true;
        } else {
            response["error"] = ErrorCode::USERNAME_OR_PASSWORD_ERROR;
            response["message"] = reply.error_msg();
            return false;
        }
    } else {
        LOG_ERROR("Verify credentials failed: {}", status.error_message());
        response["error"] = ErrorCode::RPC_ERROR;
        response["message"] = status.error_message();
        return false;
    }
}

uint32_t UserServiceClient::CreateUser(const std::string& username, const std::string& password, const std::string& email, uint64_t& uid) {
    CreateUserRequest request;
    CreateUserResponse reply;
    ClientContext context;

    request.set_username(username);
    request.set_password(password);
    request.set_email(email);

    auto stub = _stub->getStub();
    const Status status = stub->CreateUser(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (status.ok()) {
        if (reply.success()) {
            uid = reply.uid();
            return ErrorCode::SUCCESS;
        } else {
            LOG_ERROR("Create user failed: {}", reply.error_msg());
            return reply.code();
        }
    } else {
        LOG_ERROR("Create user failed: {}", status.error_message());
        return reply.code();
    }
}

bool UserServiceClient::GetUserProfile(const uint64_t uid, nlohmann::json& user_info) {
    GetUserProfileRequest request;
    GetUserProfileResponse reply;
    ClientContext context;

    request.set_uid(uid);

    auto stub = _stub->getStub();
    const Status status = stub->GetUserProfile(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (status.ok()) {
        if (reply.success()) {
            const UserProfile& profile = reply.user_profile();
            nlohmann::json profile_json;

            profile_json["uid"] = profile.uid();
            profile_json["username"] = profile.username();
            profile_json["nickname"] = profile.nickname();
            profile_json["avatar_url"] = profile.avatar_url();
            profile_json["email"] = profile.email();
            profile_json["phone"] = profile.phone();
            profile_json["gender"] = static_cast<int>(profile.gender());
            profile_json["signature"] = profile.signature();
            profile_json["location"] = profile.location();
            profile_json["status"] = static_cast<int>(profile.status());
            profile_json["create_time"] = profile.create_time();
            profile_json["update_time"] = profile.update_time();

            user_info = profile_json;
            return true;
        } else {
            LOG_ERROR("Get user profile failed: {}", reply.error_msg());
            return false;
        }
    } else {
        LOG_ERROR("Get user profile failed: {}", status.error_message());
        return false;
    }
}

bool UserServiceClient::UpdateUserProfile(const uint64_t uid, const nlohmann::json& user_info, nlohmann::json& updated_profile) {
    UpdateUserProfileRequest request;
    UpdateUserProfileResponse reply;
    ClientContext context;

    request.set_uid(uid);
    // Fix: fill fields one by one instead of using ParseFromString
    request.mutable_user_profile()->set_nickname(user_info.value("nickname", ""));
    request.mutable_user_profile()->set_avatar_url(user_info.value("avatar_url", ""));
    request.mutable_user_profile()->set_phone(user_info.value("phone", ""));
    request.mutable_user_profile()->set_gender(static_cast<UserGender>(user_info.value("gender", 0)));
    request.mutable_user_profile()->set_signature(user_info.value("signature", ""));
    request.mutable_user_profile()->set_location(user_info.value("location", ""));

    auto stub = _stub->getStub();
    const Status status = stub->UpdateUserProfile(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (status.ok()) {
        if (reply.success()) {
            // Fill updated_profile with the returned profile
            const UserProfile& profile = reply.user_profile();
            updated_profile["uid"] = profile.uid();
            updated_profile["username"] = profile.username();
            updated_profile["nickname"] = profile.nickname();
            updated_profile["avatar_url"] = profile.avatar_url();
            updated_profile["email"] = profile.email();
            updated_profile["phone"] = profile.phone();
            updated_profile["gender"] = static_cast<int>(profile.gender());
            updated_profile["signature"] = profile.signature();
            updated_profile["location"] = profile.location();
            updated_profile["status"] = static_cast<int>(profile.status());
            updated_profile["create_time"] = profile.create_time();
            updated_profile["update_time"] = profile.update_time();
            return true;
        } else {
            LOG_ERROR("Update user profile failed: {}", reply.error_msg());
            return false;
        }
    } else {
        LOG_ERROR("Update user profile failed: {}", status.error_message());
        return false;
    }
}

bool UserServiceClient::ResetPassword(const std::string& email, const std::string& new_password) {
    ResetPasswordRequest request;
    ResetPasswordResponse reply;
    ClientContext context;

    request.set_email(email);
    request.set_new_password(new_password);

    auto stub = _stub->getStub();
    const Status status = stub->ResetPassword(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (status.ok()) {
        if (reply.success()) {
            return true;
        } else {
            LOG_ERROR("Reset password failed: {}", reply.error_msg());
            return false;
        }
    } else {
        LOG_ERROR("Reset password failed: {}", status.error_message());
        return false;
    }
}

bool UserServiceClient::SearchUser(const std::string& keyword, nlohmann::json& users) {
    SearchUserRequest request;
    SearchUserResponse reply;
    ClientContext context;

    request.set_keyword(keyword);

    auto stub = _stub->getStub();
    const Status status = stub->SearchUser(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (status.ok()) {
        if (reply.success()) {
            nlohmann::json users_array = nlohmann::json::array();

            for (const auto& user : reply.users()) {
                nlohmann::json user_json;
                user_json["uid"] = user.uid();
                user_json["username"] = user.username();
                user_json["nickname"] = user.nickname();
                user_json["avatar_url"] = user.avatar_url();
                user_json["email"] = user.email();
                user_json["phone"] = user.phone();
                user_json["gender"] = static_cast<int>(user.gender());
                user_json["signature"] = user.signature();
                user_json["location"] = user.location();
                user_json["status"] = static_cast<int>(user.status());
                users_array.push_back(user_json);
            }

            users["users"] = users_array;
            return true;
        } else {
            LOG_ERROR("Search users failed: {}", reply.error_msg());
            return false;
        }
    } else {
        LOG_ERROR("Search users failed: {}", status.error_message());
        return false;
    }
}

// ========== M3: 联系人操作 ==========

bool UserServiceClient::SendContactRequest(uint64_t from_uid, uint64_t to_uid,
                                           const std::string& note, nlohmann::json& out_json) {
    SendContactRequestReq request;
    SendContactRequestResp reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    request.set_from_uid(from_uid);
    request.set_to_uid(to_uid);
    request.set_note(note);

    auto stub = _stub->getStub();
    const Status status = stub->SendContactRequest(&context, request, &reply);
    _stub->returnStub(std::move(stub));

    if (status.ok()) {
        if (reply.success()) {
            out_json["success"]    = true;
            out_json["request_id"] = reply.request_id();
            return true;
        } else {
            out_json["error"] = reply.error_msg();
            return false;
        }
    } else {
        LOG_ERROR("SendContactRequest RPC failed: {}", status.error_message());
        out_json["error"] = status.error_message();
        return false;
    }
}

bool UserServiceClient::HandleContactRequest(uint64_t request_id, uint64_t handler_uid,
                                             bool accept, nlohmann::json& out_json) {
    HandleContactRequestReq request;
    HandleContactRequestResp reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    request.set_request_id(request_id);
    request.set_handler_uid(handler_uid);
    request.set_accept(accept);

    auto stub = _stub->getStub();
    const Status status = stub->HandleContactRequest(&context, request, &reply);
    _stub->returnStub(std::move(stub));

    if (status.ok()) {
        if (reply.success()) {
            out_json["success"]  = true;
            out_json["from_uid"] = reply.from_uid();
            return true;
        } else {
            out_json["error"] = reply.error_msg();
            return false;
        }
    } else {
        LOG_ERROR("HandleContactRequest RPC failed: {}", status.error_message());
        out_json["error"] = status.error_message();
        return false;
    }
}

bool UserServiceClient::GetContacts(uint64_t uid, nlohmann::json& out_json) {
    GetContactsReq request;
    GetContactsResp reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    request.set_uid(uid);

    auto stub = _stub->getStub();
    const Status status = stub->GetContacts(&context, request, &reply);
    _stub->returnStub(std::move(stub));

    if (status.ok()) {
        if (reply.success()) {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& entry : reply.contacts()) {
                nlohmann::json obj;
                obj["uid"]        = entry.uid();
                obj["username"]   = entry.username();
                obj["nickname"]   = entry.nickname();
                obj["avatar_url"] = entry.avatar_url();
                arr.push_back(obj);
            }
            out_json["contacts"] = arr;
            return true;
        } else {
            LOG_ERROR("GetContacts failed: {}", reply.error_msg());
            return false;
        }
    } else {
        LOG_ERROR("GetContacts RPC failed: {}", status.error_message());
        return false;
    }
}

bool UserServiceClient::GetContactRequests(uint64_t uid, nlohmann::json& out_json) {
    GetContactRequestsReq request;
    GetContactRequestsResp reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    request.set_uid(uid);

    auto stub = _stub->getStub();
    const Status status = stub->GetContactRequests(&context, request, &reply);
    _stub->returnStub(std::move(stub));

    if (status.ok()) {
        if (reply.success()) {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& req : reply.requests()) {
                nlohmann::json obj;
                obj["request_id"]    = req.request_id();
                obj["from_uid"]      = req.from_uid();
                obj["from_username"] = req.from_username();
                obj["from_avatar"]   = req.from_avatar();
                obj["note"]          = req.note();
                obj["ts_ms"]         = req.ts_ms();
                arr.push_back(obj);
            }
            out_json["requests"] = arr;
            return true;
        } else {
            LOG_ERROR("GetContactRequests failed: {}", reply.error_msg());
            return false;
        }
    } else {
        LOG_ERROR("GetContactRequests RPC failed: {}", status.error_message());
        return false;
    }
}

bool UserServiceClient::CreateGroup(uint64_t owner_uid, const nlohmann::json& group_info, nlohmann::json& out_json) {
    CreateGroupReq request;
    CreateGroupResp reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    request.set_owner_uid(owner_uid);
    request.set_name(group_info.value("name", ""));
    request.set_avatar_url(group_info.value("avatar_url", ""));
    request.set_notice(group_info.value("notice", ""));
    request.set_description(group_info.value("description", ""));
    if (group_info.contains("member_uids") && group_info["member_uids"].is_array()) {
        for (const auto& uid : group_info["member_uids"]) {
            request.add_member_uids(uid.get<uint64_t>());
        }
    }

    auto stub = _stub->getStub();
    const Status status = stub->CreateGroup(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (!status.ok()) {
        out_json["error"] = status.error_message();
        return false;
    }
    if (!reply.success()) {
        out_json["error"] = reply.error_msg();
        return false;
    }

    out_json["group"] = {
        {"group_id", static_cast<int64_t>(reply.group().group_id())},
        {"owner_uid", static_cast<int64_t>(reply.group().owner_uid())},
        {"name", reply.group().name()},
        {"avatar_url", reply.group().avatar_url()},
        {"notice", reply.group().notice()},
        {"description", reply.group().description()},
        {"member_count", reply.group().member_count()},
        {"create_time", reply.group().create_time()},
        {"update_time", reply.group().update_time()}
    };
    return true;
}

bool UserServiceClient::UpdateGroup(uint64_t operator_uid, uint64_t group_id, const nlohmann::json& group_info, nlohmann::json& out_json) {
    UpdateGroupReq request;
    UpdateGroupResp reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    request.set_operator_uid(operator_uid);
    request.set_group_id(group_id);
    request.set_name(group_info.value("name", ""));
    request.set_avatar_url(group_info.value("avatar_url", ""));
    request.set_notice(group_info.value("notice", ""));
    request.set_description(group_info.value("description", ""));

    auto stub = _stub->getStub();
    const Status status = stub->UpdateGroup(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (!status.ok()) {
        out_json["error"] = status.error_message();
        return false;
    }
    if (!reply.success()) {
        out_json["error"] = reply.error_msg();
        return false;
    }

    out_json["group"] = {
        {"group_id", static_cast<int64_t>(reply.group().group_id())},
        {"owner_uid", static_cast<int64_t>(reply.group().owner_uid())},
        {"name", reply.group().name()},
        {"avatar_url", reply.group().avatar_url()},
        {"notice", reply.group().notice()},
        {"description", reply.group().description()},
        {"member_count", reply.group().member_count()},
        {"create_time", reply.group().create_time()},
        {"update_time", reply.group().update_time()}
    };
    return true;
}

bool UserServiceClient::DeleteGroup(uint64_t operator_uid, uint64_t group_id, nlohmann::json& out_json) {
    DeleteGroupReq request;
    DeleteGroupResp reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    request.set_operator_uid(operator_uid);
    request.set_group_id(group_id);

    auto stub = _stub->getStub();
    const Status status = stub->DeleteGroup(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (!status.ok()) {
        out_json["error"] = status.error_message();
        return false;
    }
    if (!reply.success()) {
        out_json["error"] = reply.error_msg();
        return false;
    }
    out_json["success"] = true;
    return true;
}

bool UserServiceClient::GetGroup(uint64_t requester_uid, uint64_t group_id, nlohmann::json& out_json) {
    GetGroupReq request;
    GetGroupResp reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    request.set_requester_uid(requester_uid);
    request.set_group_id(group_id);

    auto stub = _stub->getStub();
    const Status status = stub->GetGroup(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (!status.ok()) {
        out_json["error"] = status.error_message();
        return false;
    }
    if (!reply.success()) {
        out_json["error"] = reply.error_msg();
        return false;
    }

    out_json["group"] = {
        {"group_id", static_cast<int64_t>(reply.group().group_id())},
        {"owner_uid", static_cast<int64_t>(reply.group().owner_uid())},
        {"name", reply.group().name()},
        {"avatar_url", reply.group().avatar_url()},
        {"notice", reply.group().notice()},
        {"description", reply.group().description()},
        {"member_count", reply.group().member_count()},
        {"create_time", reply.group().create_time()},
        {"update_time", reply.group().update_time()}
    };
    return true;
}

bool UserServiceClient::SearchGroups(const std::string& keyword, uint32_t limit, nlohmann::json& out_json) {
    SearchGroupsReq request;
    SearchGroupsResp reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    request.set_keyword(keyword);
    request.set_limit(limit);

    auto stub = _stub->getStub();
    const Status status = stub->SearchGroups(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (!status.ok()) {
        out_json["error"] = status.error_message();
        return false;
    }
    if (!reply.success()) {
        out_json["error"] = reply.error_msg();
        return false;
    }

    nlohmann::json groups = nlohmann::json::array();
    for (const auto& group : reply.groups()) {
        groups.push_back({
            {"group_id", static_cast<int64_t>(group.group_id())},
            {"owner_uid", static_cast<int64_t>(group.owner_uid())},
            {"name", group.name()},
            {"avatar_url", group.avatar_url()},
            {"notice", group.notice()},
            {"description", group.description()},
            {"member_count", group.member_count()},
            {"create_time", group.create_time()},
            {"update_time", group.update_time()}
        });
    }
    out_json["groups"] = groups;
    return true;
}

bool UserServiceClient::ListMyGroups(uint64_t uid, nlohmann::json& out_json) {
    ListMyGroupsReq request;
    ListMyGroupsResp reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    request.set_uid(uid);

    auto stub = _stub->getStub();
    const Status status = stub->ListMyGroups(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (!status.ok()) {
        out_json["error"] = status.error_message();
        return false;
    }
    if (!reply.success()) {
        out_json["error"] = reply.error_msg();
        return false;
    }

    nlohmann::json groups = nlohmann::json::array();
    for (const auto& group : reply.groups()) {
        groups.push_back({
            {"group_id", static_cast<int64_t>(group.group_id())},
            {"owner_uid", static_cast<int64_t>(group.owner_uid())},
            {"name", group.name()},
            {"avatar_url", group.avatar_url()},
            {"notice", group.notice()},
            {"description", group.description()},
            {"member_count", group.member_count()},
            {"create_time", group.create_time()},
            {"update_time", group.update_time()}
        });
    }
    out_json["groups"] = groups;
    return true;
}

bool UserServiceClient::JoinGroup(uint64_t uid, uint64_t group_id, nlohmann::json& out_json) {
    JoinGroupReq request;
    JoinGroupResp reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    request.set_uid(uid);
    request.set_group_id(group_id);

    auto stub = _stub->getStub();
    const Status status = stub->JoinGroup(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (!status.ok()) {
        out_json["error"] = status.error_message();
        return false;
    }
    if (!reply.success()) {
        out_json["error"] = reply.error_msg();
        return false;
    }
    out_json["success"] = true;
    return true;
}

bool UserServiceClient::QuitGroup(uint64_t uid, uint64_t group_id, nlohmann::json& out_json) {
    QuitGroupReq request;
    QuitGroupResp reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    request.set_uid(uid);
    request.set_group_id(group_id);

    auto stub = _stub->getStub();
    const Status status = stub->QuitGroup(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (!status.ok()) {
        out_json["error"] = status.error_message();
        return false;
    }
    if (!reply.success()) {
        out_json["error"] = reply.error_msg();
        return false;
    }
    out_json["success"] = true;
    return true;
}

bool UserServiceClient::GetGroupMembers(uint64_t requester_uid, uint64_t group_id, nlohmann::json& out_json) {
    GetGroupMembersReq request;
    GetGroupMembersResp reply;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    request.set_requester_uid(requester_uid);
    request.set_group_id(group_id);

    auto stub = _stub->getStub();
    const Status status = stub->GetGroupMembers(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (!status.ok()) {
        out_json["error"] = status.error_message();
        return false;
    }
    if (!reply.success()) {
        out_json["error"] = reply.error_msg();
        return false;
    }

    nlohmann::json members = nlohmann::json::array();
    for (const auto& member : reply.members()) {
        members.push_back({
            {"uid", static_cast<int64_t>(member.uid())},
            {"username", member.username()},
            {"nickname", member.nickname()},
            {"avatar_url", member.avatar_url()},
            {"role", static_cast<int>(member.role())},
            {"join_time", member.join_time()}
        });
    }
    out_json["members"] = members;
    return true;
}
