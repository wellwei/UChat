#pragma once

#include <grpcpp/grpcpp.h>
#include "userserver.grpc.pb.h"

class UserServer final : public UserService::Service {
public:
    grpc::Status VerifyCredentials(grpc::ServerContext* context, const VerifyCredentialsRequest* request, VerifyCredentialsResponse* response) override;
    grpc::Status CreateUser(grpc::ServerContext* context, const CreateUserRequest* request, CreateUserResponse* response) override;
    grpc::Status GetUserProfile(grpc::ServerContext* context, const GetUserProfileRequest* request, GetUserProfileResponse* response) override;
    grpc::Status UpdateUserProfile(grpc::ServerContext* context, const UpdateUserProfileRequest* request, UpdateUserProfileResponse* response) override;
    grpc::Status ResetPassword(grpc::ServerContext* context, const ResetPasswordRequest* request, ResetPasswordResponse* response) override;

    grpc::Status SearchUser(grpc::ServerContext* context, const SearchUserRequest* request, SearchUserResponse* response) override;

    void start();
    void stop();
    
private:
    std::unique_ptr<grpc::Server> server_;  

    enum ErrorCode {
        USER_EXISTS = 1006,
        SUCCESS = 0,
    };
};
