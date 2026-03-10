#pragma once

#include <grpcpp/grpcpp.h>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>
#include "im.grpc.pb.h"
#include "GrpcStubPool.h"

// Per-gateway gRPC stub pool for DeliverApi calls.
// Pools are created lazily on first use per gateway address.
class DeliverApiClient {
public:
    DeliverApiClient() = default;
    ~DeliverApiClient() = default;

    // Deliver a message to a user on the given gateway.
    // Returns true if the RPC succeeded (online/ok may still be false).
    bool DeliverToUser(const std::string& gateway_addr,
                       const im::DeliverToUserReq& req,
                       im::DeliverToUserResp& resp);

private:
    GrpcStubPool<im::DeliverApi>* GetOrCreatePool(const std::string& addr);

    std::mutex mu_;
    std::unordered_map<std::string, std::unique_ptr<GrpcStubPool<im::DeliverApi>>> pools_;
};
