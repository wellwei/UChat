#pragma once

#include <grpcpp/grpcpp.h>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>
#include <functional>
#include "im.grpc.pb.h"
#include "GrpcStubPool.h"

// Per-gateway gRPC stub pool for DeliverApi calls.
// Pools are created lazily on first use per gateway address.
class DeliverApiClient {
public:
    DeliverApiClient() = default;
    ~DeliverApiClient() = default;

    // Synchronous delivery (DEPRECATED: use Async version for fire-and-forget)
    // Returns true if the RPC succeeded (online/ok may still be false).
    bool DeliverToUser(const std::string& gateway_addr,
                       const im::DeliverToUserReq& req,
                       im::DeliverToUserResp& resp);

    // Fire-and-forget async delivery.
    // Does NOT wait for response. Returns immediately.
    // Message is already persisted in offline queue before calling this.
    void DeliverToUserAsync(const std::string& gateway_addr,
                            const im::DeliverToUserReq& req);

private:
    GrpcStubPool<im::DeliverApi>* GetOrCreatePool(const std::string& addr);

    std::mutex mu_;
    std::unordered_map<std::string, std::unique_ptr<GrpcStubPool<im::DeliverApi>>> pools_;
};
