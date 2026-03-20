#include "ChatControlApiService.h"
#include "UserServiceClient.h"
#include "ChatApiClient.h"
#include "Logger.h"
#include <nlohmann/json.hpp>

// Helper to convert JSON ProfileData to protobuf
static void JsonToProfileData(const nlohmann::json& j, im::ProfileData* profile) {
    profile->set_uid(j.value("uid", int64_t(0)));
    profile->set_username(j.value("username", ""));
    profile->set_nickname(j.value("nickname", ""));
    profile->set_avatar_url(j.value("avatar_url", ""));
    profile->set_email(j.value("email", ""));
    profile->set_phone(j.value("phone", ""));
    profile->set_gender(j.value("gender", 0));
    profile->set_signature(j.value("signature", ""));
    profile->set_location(j.value("location", ""));
    profile->set_status(j.value("status", 0));
    profile->set_create_time(j.value("create_time", ""));
    profile->set_update_time(j.value("update_time", ""));
}

// Helper to convert JSON ContactEntryData to protobuf
static void JsonToContactEntry(const nlohmann::json& j, im::ContactEntryData* contact) {
    contact->set_uid(j.value("uid", int64_t(0)));
    contact->set_username(j.value("username", ""));
    contact->set_nickname(j.value("nickname", ""));
    contact->set_avatar_url(j.value("avatar_url", ""));
}

// Helper to convert JSON ContactRequestData to protobuf
static void JsonToContactRequest(const nlohmann::json& j, im::ContactRequestData* req) {
    req->set_request_id(j.value("request_id", int64_t(0)));
    req->set_from_uid(j.value("from_uid", int64_t(0)));
    req->set_from_username(j.value("from_username", ""));
    req->set_from_avatar(j.value("from_avatar", ""));
    req->set_note(j.value("note", ""));
    req->set_ts_ms(j.value("ts_ms", int64_t(0)));
}

static void JsonToGroupData(const nlohmann::json& j, im::GroupData* group) {
    group->set_group_id(j.value("group_id", int64_t(0)));
    group->set_owner_uid(j.value("owner_uid", int64_t(0)));
    group->set_name(j.value("name", ""));
    group->set_avatar_url(j.value("avatar_url", ""));
    group->set_notice(j.value("notice", ""));
    group->set_description(j.value("description", ""));
    group->set_member_count(j.value("member_count", 0U));
    group->set_create_time(j.value("create_time", ""));
    group->set_update_time(j.value("update_time", ""));
}

static void JsonToGroupMemberData(const nlohmann::json& j, im::GroupMemberData* member) {
    member->set_uid(j.value("uid", int64_t(0)));
    member->set_username(j.value("username", ""));
    member->set_nickname(j.value("nickname", ""));
    member->set_avatar_url(j.value("avatar_url", ""));
    member->set_role(j.value("role", 0));
    member->set_join_time(j.value("join_time", ""));
}

// Extract uid from client metadata
// Client should set "uid" and "token" metadata for authenticated requests
static int64_t GetUidFromMetadata(grpc::ServerContext* context) {
    auto metadata = context->client_metadata();
    auto it = metadata.find("uid");
    if (it != metadata.end()) {
        std::string uid_str(it->second.data(), it->second.size());
        try {
            return std::stoll(uid_str);
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

// ==================== Sync ====================
grpc::Status ChatControlApiService::Sync(grpc::ServerContext* context,
                                         const im::SyncReq* request,
                                         im::SyncResp* response) {
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    // Forward to ChatServer
    im::SyncUpReq up_req;
    up_req.set_uid(uid);
    up_req.set_last_inbox_seq(request->last_inbox_seq());
    up_req.set_limit(request->limit());

    auto up_resp = ChatApiClient::getInstance()->Sync(up_req);

    response->set_ok(up_resp.ok());
    response->set_message("");
    for (const auto& env : up_resp.msgs()) {
        *response->add_msgs() = env;
    }
    response->set_max_inbox_seq(up_resp.max_inbox_seq());
    response->set_has_more(up_resp.has_more());

    LOG_DEBUG("ChatControlApiService::Sync: uid={}, from_seq={}, msgs={}, max_seq={}, has_more={}",
              uid, request->last_inbox_seq(), response->msgs_size(),
              response->max_inbox_seq(), response->has_more());
    return grpc::Status::OK;
}

// ==================== Auth Operations ====================
grpc::Status ChatControlApiService::GetVerifyCode(grpc::ServerContext* context,
                                                  const im::GetVerifyCodeReq* request,
                                                  im::GetVerifyCodeResp* response) {
    // TODO: Implement email verification code
    response->set_ok(false);
    response->set_message("not implemented");
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
}

grpc::Status ChatControlApiService::Register(grpc::ServerContext* context,
                                             const im::RegisterReq* request,
                                             im::RegisterResp* response) {
    uint64_t uid = 0;
    uint32_t result = UserServiceClient::getInstance()->CreateUser(
        request->username(),
        request->password(),
        request->email(),
        uid);

    if (result == 0) {
        response->set_ok(true);
        response->set_message("success");
        response->set_uid(static_cast<int64_t>(uid));
        LOG_INFO("ChatControlApiService::Register: success uid={}", uid);
    } else {
        response->set_ok(false);
        response->set_message("register failed");
        response->set_uid(0);
        LOG_WARN("ChatControlApiService::Register: failed code={}", result);
    }
    return grpc::Status::OK;
}

grpc::Status ChatControlApiService::AuthLogin(grpc::ServerContext* context,
                                              const im::AuthLoginReq* request,
                                              im::AuthLoginResp* response) {
    nlohmann::json resp_json;
    bool ok = UserServiceClient::getInstance()->VerifyCredentials(
        request->handle(),
        request->password(),
        resp_json);

    if (ok && resp_json.value("success", false)) {
        response->set_ok(true);
        response->set_message("success");
        response->set_uid(resp_json.value("uid", int64_t(0)));
        response->set_token(resp_json.value("token", ""));
        LOG_INFO("ChatControlApiService::AuthLogin: success uid={}", response->uid());
    } else {
        response->set_ok(false);
        response->set_message(resp_json.value("message", "login failed"));
        response->set_uid(0);
        response->set_token("");
        LOG_WARN("ChatControlApiService::AuthLogin: failed");
    }
    return grpc::Status::OK;
}

grpc::Status ChatControlApiService::ResetPassword(grpc::ServerContext* context,
                                                  const im::ResetPasswordReq* request,
                                                  im::ResetPasswordResp* response) {
    bool ok = UserServiceClient::getInstance()->ResetPassword(
        request->email(),
        request->new_password());

    response->set_ok(ok);
    response->set_message(ok ? "success" : "failed");
    return grpc::Status::OK;
}

// ==================== Profile Operations ====================
grpc::Status ChatControlApiService::GetUserProfile(grpc::ServerContext* context,
                                                  const im::GetUserProfileReq* request,
                                                  im::GetUserProfileResp* response) {
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    // target_uid = 0 means self
    uint64_t target_uid = request->target_uid() > 0 ? request->target_uid() : uid;

    nlohmann::json profile_json;
    bool ok = UserServiceClient::getInstance()->GetUserProfile(target_uid, profile_json);

    if (ok) {
        response->set_ok(true);
        response->set_message("success");
        JsonToProfileData(profile_json, response->mutable_profile());
    } else {
        response->set_ok(false);
        response->set_message("user not found");
    }
    return grpc::Status::OK;
}

grpc::Status ChatControlApiService::UpdateUserProfile(grpc::ServerContext* context,
                                                      const im::UpdateUserProfileReq* request,
                                                      im::UpdateUserProfileResp* response) {
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    nlohmann::json update_json;
    update_json["nickname"] = request->nickname();
    update_json["avatar_url"] = request->avatar_url();
    update_json["phone"] = request->phone();
    update_json["gender"] = request->gender();
    update_json["signature"] = request->signature();
    update_json["location"] = request->location();

    nlohmann::json updated_profile;
    bool ok = UserServiceClient::getInstance()->UpdateUserProfile(uid, update_json, updated_profile);

    if (ok) {
        response->set_ok(true);
        response->set_message("success");
        JsonToProfileData(updated_profile, response->mutable_profile());
    } else {
        response->set_ok(false);
        response->set_message("update failed");
    }
    return grpc::Status::OK;
}

grpc::Status ChatControlApiService::SearchUser(grpc::ServerContext* context,
                                              const im::SearchUserReq* request,
                                              im::SearchUserResp* response) {
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    nlohmann::json users_json;
    bool ok = UserServiceClient::getInstance()->SearchUser(request->keyword(), users_json);

    if (ok && users_json.contains("users") && users_json["users"].is_array()) {
        response->set_ok(true);
        response->set_message("success");
        for (const auto& user : users_json["users"]) {
            JsonToProfileData(user, response->add_profiles());
        }
    } else {
        response->set_ok(false);
        response->set_message("search failed");
    }
    return grpc::Status::OK;
}

// ==================== Contact Operations ====================
grpc::Status ChatControlApiService::GetContacts(grpc::ServerContext* context,
                                                const im::GetContactsReq* request,
                                                im::GetContactsResp* response) {
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    nlohmann::json contacts_json;
    bool ok = UserServiceClient::getInstance()->GetContacts(uid, contacts_json);

    if (ok && contacts_json.contains("contacts") && contacts_json["contacts"].is_array()) {
        response->set_ok(true);
        response->set_message("success");
        for (const auto& contact : contacts_json["contacts"]) {
            JsonToContactEntry(contact, response->add_contacts());
        }
    } else {
        response->set_ok(false);
        response->set_message("get contacts failed");
    }
    return grpc::Status::OK;
}

grpc::Status ChatControlApiService::GetContactRequests(grpc::ServerContext* context,
                                                      const im::GetContactRequestsReq* request,
                                                      im::GetContactRequestsResp* response) {
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    nlohmann::json requests_json;
    bool ok = UserServiceClient::getInstance()->GetContactRequests(uid, requests_json);

    if (ok && requests_json.contains("requests") && requests_json["requests"].is_array()) {
        response->set_ok(true);
        response->set_message("success");
        for (const auto& req : requests_json["requests"]) {
            JsonToContactRequest(req, response->add_requests());
        }
    } else {
        response->set_ok(false);
        response->set_message("get contact requests failed");
    }
    return grpc::Status::OK;
}

grpc::Status ChatControlApiService::SendContactRequest(grpc::ServerContext* context,
                                                       const im::SendContactReqReq* request,
                                                       im::SendContactReqResp* response) {
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    nlohmann::json result_json;
    bool ok = UserServiceClient::getInstance()->SendContactRequest(
        uid, request->to_uid(), request->note(), result_json);

    response->set_ok(ok);
    response->set_message(ok ? "success" : "send contact request failed");
    return grpc::Status::OK;
}

grpc::Status ChatControlApiService::HandleContactRequest(grpc::ServerContext* context,
                                                         const im::HandleContactReqReq* request,
                                                         im::HandleContactReqResp* response) {
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    nlohmann::json result_json;
    bool ok = UserServiceClient::getInstance()->HandleContactRequest(
        request->request_id(), uid, request->accept(), result_json);

    response->set_ok(ok);
    response->set_message(ok ? "success" : "handle contact request failed");
    return grpc::Status::OK;
}

grpc::Status ChatControlApiService::CreateGroup(grpc::ServerContext* context,
                                                const im::CreateGroupReq* request,
                                                im::CreateGroupResp* response) {
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    nlohmann::json req_json;
    req_json["name"] = request->name();
    req_json["avatar_url"] = request->avatar_url();
    req_json["notice"] = request->notice();
    req_json["description"] = request->description();
    req_json["member_uids"] = nlohmann::json::array();
    for (const auto member_uid : request->member_uids()) {
        req_json["member_uids"].push_back(member_uid);
    }

    nlohmann::json result_json;
    bool ok = UserServiceClient::getInstance()->CreateGroup(uid, req_json, result_json);
    response->set_ok(ok);
    response->set_message(ok ? "success" : result_json.value("error", "create group failed"));
    if (ok && result_json.contains("group")) {
        JsonToGroupData(result_json["group"], response->mutable_group());
    }
    return grpc::Status::OK;
}

grpc::Status ChatControlApiService::UpdateGroup(grpc::ServerContext* context,
                                                const im::UpdateGroupReq* request,
                                                im::UpdateGroupResp* response) {
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    nlohmann::json req_json;
    req_json["name"] = request->name();
    req_json["avatar_url"] = request->avatar_url();
    req_json["notice"] = request->notice();
    req_json["description"] = request->description();

    nlohmann::json result_json;
    bool ok = UserServiceClient::getInstance()->UpdateGroup(uid, request->group_id(), req_json, result_json);
    response->set_ok(ok);
    response->set_message(ok ? "success" : result_json.value("error", "update group failed"));
    if (ok && result_json.contains("group")) {
        JsonToGroupData(result_json["group"], response->mutable_group());
    }
    return grpc::Status::OK;
}

grpc::Status ChatControlApiService::DeleteGroup(grpc::ServerContext* context,
                                                const im::DeleteGroupReq* request,
                                                im::DeleteGroupResp* response) {
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    nlohmann::json result_json;
    const bool ok = UserServiceClient::getInstance()->DeleteGroup(uid, request->group_id(), result_json);
    response->set_ok(ok);
    response->set_message(ok ? "success" : result_json.value("error", "delete group failed"));
    return grpc::Status::OK;
}

grpc::Status ChatControlApiService::GetGroup(grpc::ServerContext* context,
                                             const im::GetGroupReq* request,
                                             im::GetGroupResp* response) {
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    nlohmann::json result_json;
    const bool ok = UserServiceClient::getInstance()->GetGroup(uid, request->group_id(), result_json);
    response->set_ok(ok);
    response->set_message(ok ? "success" : result_json.value("error", "get group failed"));
    if (ok && result_json.contains("group")) {
        JsonToGroupData(result_json["group"], response->mutable_group());
    }
    return grpc::Status::OK;
}

grpc::Status ChatControlApiService::SearchGroups(grpc::ServerContext* context,
                                                 const im::SearchGroupsReq* request,
                                                 im::SearchGroupsResp* response) {
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    nlohmann::json result_json;
    const bool ok = UserServiceClient::getInstance()->SearchGroups(request->keyword(), request->limit(), result_json);
    response->set_ok(ok);
    response->set_message(ok ? "success" : result_json.value("error", "search groups failed"));
    if (ok && result_json.contains("groups") && result_json["groups"].is_array()) {
        for (const auto& group : result_json["groups"]) {
            JsonToGroupData(group, response->add_groups());
        }
    }
    return grpc::Status::OK;
}

grpc::Status ChatControlApiService::ListMyGroups(grpc::ServerContext* context,
                                                 const im::ListMyGroupsReq* request,
                                                 im::ListMyGroupsResp* response) {
    (void)request;
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    nlohmann::json result_json;
    const bool ok = UserServiceClient::getInstance()->ListMyGroups(uid, result_json);
    response->set_ok(ok);
    response->set_message(ok ? "success" : result_json.value("error", "list groups failed"));
    if (ok && result_json.contains("groups") && result_json["groups"].is_array()) {
        for (const auto& group : result_json["groups"]) {
            JsonToGroupData(group, response->add_groups());
        }
    }
    return grpc::Status::OK;
}

grpc::Status ChatControlApiService::JoinGroup(grpc::ServerContext* context,
                                              const im::JoinGroupReq* request,
                                              im::JoinGroupResp* response) {
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    nlohmann::json result_json;
    const bool ok = UserServiceClient::getInstance()->JoinGroup(uid, request->group_id(), result_json);
    response->set_ok(ok);
    response->set_message(ok ? "success" : result_json.value("error", "join group failed"));
    return grpc::Status::OK;
}

grpc::Status ChatControlApiService::QuitGroup(grpc::ServerContext* context,
                                              const im::QuitGroupReq* request,
                                              im::QuitGroupResp* response) {
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    nlohmann::json result_json;
    const bool ok = UserServiceClient::getInstance()->QuitGroup(uid, request->group_id(), result_json);
    response->set_ok(ok);
    response->set_message(ok ? "success" : result_json.value("error", "quit group failed"));
    return grpc::Status::OK;
}

grpc::Status ChatControlApiService::GetGroupMembers(grpc::ServerContext* context,
                                                    const im::GetGroupMembersReq* request,
                                                    im::GetGroupMembersResp* response) {
    int64_t uid = GetUidFromMetadata(context);
    if (uid <= 0) {
        response->set_ok(false);
        response->set_message("unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing uid");
    }

    nlohmann::json result_json;
    const bool ok = UserServiceClient::getInstance()->GetGroupMembers(uid, request->group_id(), result_json);
    response->set_ok(ok);
    response->set_message(ok ? "success" : result_json.value("error", "get group members failed"));
    if (ok && result_json.contains("members") && result_json["members"].is_array()) {
        for (const auto& member : result_json["members"]) {
            JsonToGroupMemberData(member, response->add_members());
        }
    }
    return grpc::Status::OK;
}
