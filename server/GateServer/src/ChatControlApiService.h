#pragma once

#include "im.grpc.pb.h"
#include <grpcpp/grpcpp.h>

// ChatControlApiService implements the unary RPC control interface.
// All control operations (auth, profile, contacts, sync) go through this service
// instead of being multiplexed through the bidirectional stream.
class ChatControlApiService : public im::ChatControlApi::Service {
public:
    ChatControlApiService() = default;

    // Sync: Get offline messages
    grpc::Status Sync(grpc::ServerContext* context,
                      const im::SyncReq* request,
                      im::SyncResp* response) override;

    // Auth operations (no auth required)
    grpc::Status GetVerifyCode(grpc::ServerContext* context,
                               const im::GetVerifyCodeReq* request,
                               im::GetVerifyCodeResp* response) override;

    grpc::Status Register(grpc::ServerContext* context,
                          const im::RegisterReq* request,
                          im::RegisterResp* response) override;

    grpc::Status AuthLogin(grpc::ServerContext* context,
                           const im::AuthLoginReq* request,
                           im::AuthLoginResp* response) override;

    grpc::Status ResetPassword(grpc::ServerContext* context,
                               const im::ResetPasswordReq* request,
                               im::ResetPasswordResp* response) override;

    // Profile operations (auth required)
    grpc::Status GetUserProfile(grpc::ServerContext* context,
                                const im::GetUserProfileReq* request,
                                im::GetUserProfileResp* response) override;

    grpc::Status UpdateUserProfile(grpc::ServerContext* context,
                                   const im::UpdateUserProfileReq* request,
                                   im::UpdateUserProfileResp* response) override;

    grpc::Status SearchUser(grpc::ServerContext* context,
                            const im::SearchUserReq* request,
                            im::SearchUserResp* response) override;

    // Contact operations (auth required)
    grpc::Status GetContacts(grpc::ServerContext* context,
                             const im::GetContactsReq* request,
                             im::GetContactsResp* response) override;

    grpc::Status GetContactRequests(grpc::ServerContext* context,
                                    const im::GetContactRequestsReq* request,
                                    im::GetContactRequestsResp* response) override;

    grpc::Status SendContactRequest(grpc::ServerContext* context,
                                    const im::SendContactReqReq* request,
                                    im::SendContactReqResp* response) override;

    grpc::Status HandleContactRequest(grpc::ServerContext* context,
                                      const im::HandleContactReqReq* request,
                                      im::HandleContactReqResp* response) override;

    grpc::Status CreateGroup(grpc::ServerContext* context,
                             const im::CreateGroupReq* request,
                             im::CreateGroupResp* response) override;

    grpc::Status UpdateGroup(grpc::ServerContext* context,
                             const im::UpdateGroupReq* request,
                             im::UpdateGroupResp* response) override;

    grpc::Status DeleteGroup(grpc::ServerContext* context,
                             const im::DeleteGroupReq* request,
                             im::DeleteGroupResp* response) override;

    grpc::Status GetGroup(grpc::ServerContext* context,
                          const im::GetGroupReq* request,
                          im::GetGroupResp* response) override;

    grpc::Status SearchGroups(grpc::ServerContext* context,
                              const im::SearchGroupsReq* request,
                              im::SearchGroupsResp* response) override;

    grpc::Status ListMyGroups(grpc::ServerContext* context,
                              const im::ListMyGroupsReq* request,
                              im::ListMyGroupsResp* response) override;

    grpc::Status JoinGroup(grpc::ServerContext* context,
                           const im::JoinGroupReq* request,
                           im::JoinGroupResp* response) override;

    grpc::Status QuitGroup(grpc::ServerContext* context,
                           const im::QuitGroupReq* request,
                           im::QuitGroupResp* response) override;

    grpc::Status GetGroupMembers(grpc::ServerContext* context,
                                 const im::GetGroupMembersReq* request,
                                 im::GetGroupMembersResp* response) override;

private:
    // Helper to extract uid from metadata (set by interceptor)
    int64_t GetUidFromContext(grpc::ServerContext* context);

    // Helper to check if request is authenticated
    bool IsAuthenticated(grpc::ServerContext* context);
};
