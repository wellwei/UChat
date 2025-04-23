#pragma once

#include <grpcpp/grpcpp.h>
#include "proto_generated/user_service.grpc.pb.h"

class UserServer : public UserService::Service {
public:
    grpc::Status VerifyCredentials(grpc::ServerContext* context, const VerifyCredentialsRequest* request, VerifyCredentialsResponse* response) override;
    grpc::Status CreateUser(grpc::ServerContext* context, const CreateUserRequest* request, CreateUserResponse* response) override;
    grpc::Status GetUserProfile(grpc::ServerContext* context, const GetUserProfileRequest* request, GetUserProfileResponse* response) override;
    grpc::Status UpdateUserProfile(grpc::ServerContext* context, const UpdateUserProfileRequest* request, UpdateUserProfileResponse* response) override;

    void start();
    void stop();

private:
    std::unique_ptr<grpc::Server> server_;  
};
