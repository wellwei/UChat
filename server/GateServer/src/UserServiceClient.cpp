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

bool UserServiceClient::CreateUser(const std::string& username, const std::string& password, const std::string& email, uint64_t& uid) {
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
            return true;
        } else {
            LOG_ERROR("Create user failed: {}", reply.error_msg());
            return false;
        }
    } else {
        LOG_ERROR("Create user failed: {}", status.error_message());
        return false;
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

bool UserServiceClient::UpdateUserProfile(const uint64_t uid, const nlohmann::json& user_info) {
    UpdateUserProfileRequest request;
    UpdateUserProfileResponse reply;
    ClientContext context;

    request.set_uid(uid);
    request.mutable_user_profile()->ParseFromString(user_info.dump());

    auto stub = _stub->getStub();
    const Status status = stub->UpdateUserProfile(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (status.ok()) {
        if (reply.success()) {
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

bool UserServiceClient::AddContact(const uint64_t user_id, const uint64_t friend_id, nlohmann::json& response) {
    AddContactRequest request;
    AddContactResponse reply;
    ClientContext context;

    request.set_user_id(user_id);
    request.set_friend_id(friend_id);

    auto stub = _stub->getStub();
    const Status status = stub->AddContact(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (status.ok()) {
        if (reply.success()) {
            return true;
        } else {
            response["error"] = ErrorCode::RPC_ERROR;
            response["message"] = reply.error_msg();
            LOG_ERROR("Add contact failed: {}", reply.error_msg());
            return false;
        }
    } else {
        response["error"] = ErrorCode::RPC_ERROR;
        response["message"] = status.error_message();
        LOG_ERROR("Add contact failed: {}", status.error_message());
        return false;
    }
}

bool UserServiceClient::GetContacts(const uint64_t uid, nlohmann::json& contacts) {
    GetContactsRequest request;
    GetContactsResponse reply;
    ClientContext context;

    request.set_uid(uid);

    auto stub = _stub->getStub();
    const Status status = stub->GetContacts(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (status.ok()) {
        if (reply.success()) {
            nlohmann::json contacts_array = nlohmann::json::array();
            
            for (const auto& contact : reply.contacts()) {
                nlohmann::json contact_json;
                contact_json["uid"] = contact.uid();
                contact_json["username"] = contact.username();
                contact_json["nickname"] = contact.nickname();
                contact_json["avatar_url"] = contact.avatar_url();
                contact_json["status"] = static_cast<int>(contact.status());
                contact_json["signature"] = contact.signature();
                contacts_array.push_back(contact_json);
            }
            
            contacts = contacts_array;
            return true;
        } else {
            LOG_ERROR("Get contacts failed: {}", reply.error_msg());
            return false;
        }
    } else {
        LOG_ERROR("Get contacts failed: {}", status.error_message());
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
            
            users = users_array;
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