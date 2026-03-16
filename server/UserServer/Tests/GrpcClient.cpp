#include "GrpcClient.h"
#include <grpcpp/create_channel.h>
#include "userserver.grpc.pb.h"

GrpcClient::GrpcClient(const std::string& target)
    : target_(target) {
    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    stub_ = UserService::NewStub(channel);
}

GrpcClient::~GrpcClient() = default;

bool GrpcClient::GetUserProfile(uint64_t uid) {
    GetUserProfileRequest request;
    request.set_uid(uid);

    GetUserProfileResponse reply;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    const auto status = stub_->GetUserProfile(&context, request, &reply);
    return status.ok() && reply.success();
}

bool GrpcClient::SearchUser(const std::string& keyword) {
    SearchUserRequest request;
    request.set_keyword(keyword);

    SearchUserResponse reply;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    const auto status = stub_->SearchUser(&context, request, &reply);
    return status.ok() && reply.success();
}
