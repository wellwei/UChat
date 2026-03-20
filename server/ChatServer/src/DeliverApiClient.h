#pragma once

#include <grpcpp/grpcpp.h>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>
#include <functional>
#include "im.grpc.pb.h"
#include "ChannelPool.h"

// Per-gateway gRPC stub pool for DeliverApi calls.
// Pools are created lazily on first use per gateway address.
class DeliverApiClient {
public:
    DeliverApiClient() = default;
    ~DeliverApiClient() = default;

    bool DeliverToUser(const std::string& gateway_addr,
                       const im::DeliverToUserReq& req);


private:
    ChannelPool channel_pool_;
};
