#include "UserServiceClient.h"
#include "ConfigMgr.h"
#include "Logger.h"
#include <chrono>

UserServiceClient::UserServiceClient() {
    auto& config = ConfigMgr::getInstance();
    server_address_ = config["UserServer"].get("host", "localhost") + ":" +
                      config["UserServer"].get("port", "50053");
}

bool UserServiceClient::GetGroupMemberUids(uint64_t requester_uid, uint64_t group_id, std::vector<int64_t>& member_uids) {
    if (requester_uid == 0 || group_id == 0) {
        return false;
    }

    auto stub = UserService::NewStub(channel_pool_.GetChannel(server_address_));
    if (!stub) {
        LOG_ERROR("UserServiceClient::GetGroupMemberUids no stub for {}", server_address_);
        return false;
    }

    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(200));

    GetGroupMembersReq request;
    GetGroupMembersResp response;
    request.set_requester_uid(requester_uid);
    request.set_group_id(group_id);

    const auto status = stub->GetGroupMembers(&context, request, &response);
    if (!status.ok()) {
        LOG_WARN("UserServiceClient::GetGroupMemberUids rpc failed group_id={} requester_uid={} err={}",
                 group_id, requester_uid, status.error_message());
        return false;
    }
    if (!response.success()) {
        LOG_WARN("UserServiceClient::GetGroupMemberUids failed group_id={} requester_uid={} reason={}",
                 group_id, requester_uid, response.error_msg());
        return false;
    }

    member_uids.clear();
    member_uids.reserve(static_cast<size_t>(response.members_size()));
    for (const auto& member : response.members()) {
        member_uids.push_back(static_cast<int64_t>(member.uid()));
    }
    return true;
}
