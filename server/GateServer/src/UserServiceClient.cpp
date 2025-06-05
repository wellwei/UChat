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
            response["user_profile"] = reply.user_profile().SerializeAsString();
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
            user_info = reply.user_profile().SerializeAsString();
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