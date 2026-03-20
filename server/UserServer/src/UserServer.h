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

    // M3: 联系人操作
    grpc::Status SendContactRequest(grpc::ServerContext* context, const SendContactRequestReq* request, SendContactRequestResp* response) override;
    grpc::Status HandleContactRequest(grpc::ServerContext* context, const HandleContactRequestReq* request, HandleContactRequestResp* response) override;
    grpc::Status GetContacts(grpc::ServerContext* context, const GetContactsReq* request, GetContactsResp* response) override;
    grpc::Status GetContactRequests(grpc::ServerContext* context, const GetContactRequestsReq* request, GetContactRequestsResp* response) override;

    // M4: 群聊操作
    grpc::Status CreateGroup(grpc::ServerContext* context, const CreateGroupReq* request, CreateGroupResp* response) override;
    grpc::Status UpdateGroup(grpc::ServerContext* context, const UpdateGroupReq* request, UpdateGroupResp* response) override;
    grpc::Status DeleteGroup(grpc::ServerContext* context, const DeleteGroupReq* request, DeleteGroupResp* response) override;
    grpc::Status GetGroup(grpc::ServerContext* context, const GetGroupReq* request, GetGroupResp* response) override;
    grpc::Status SearchGroups(grpc::ServerContext* context, const SearchGroupsReq* request, SearchGroupsResp* response) override;
    grpc::Status ListMyGroups(grpc::ServerContext* context, const ListMyGroupsReq* request, ListMyGroupsResp* response) override;
    grpc::Status JoinGroup(grpc::ServerContext* context, const JoinGroupReq* request, JoinGroupResp* response) override;
    grpc::Status QuitGroup(grpc::ServerContext* context, const QuitGroupReq* request, QuitGroupResp* response) override;
    grpc::Status GetGroupMembers(grpc::ServerContext* context, const GetGroupMembersReq* request, GetGroupMembersResp* response) override;

    void start();
    void stop();
    
private:
    std::unique_ptr<grpc::Server> server_;  

    enum ErrorCode {
        USER_EXISTS = 1006,
        SUCCESS = 0,
    };
};
