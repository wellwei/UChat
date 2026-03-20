#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "ChannelPool.h"
#include "userserver.grpc.pb.h"

class UserServiceClient {
public:
    UserServiceClient();
    ~UserServiceClient() = default;

    bool GetGroupMemberUids(uint64_t requester_uid, uint64_t group_id, std::vector<int64_t>& member_uids);

private:
    ChannelPool channel_pool_;
    std::string server_address_;
};
