#include "DeliverApiClient.h"
#include "Logger.h"
#include <chrono>
#include <thread>

bool DeliverApiClient::DeliverToUser(const std::string& gateway_addr,
                                     const im::DeliverToUserReq& req,
                                     im::DeliverToUserResp& resp) {
    auto* pool = GetOrCreatePool(gateway_addr);
    auto stub = pool->getStub();
    if (!stub) {
        LOG_ERROR("DeliverApiClient: no stub available for {}", gateway_addr);
        return false;
    }

    grpc::ClientContext ctx;
    ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(50));

    auto status = stub->DeliverToUser(&ctx, req, &resp);
    pool->returnStub(std::move(stub));

    if (!status.ok()) {
        LOG_WARN("DeliverApiClient: RPC failed for gateway={}: {}", gateway_addr, status.error_message());
        return false;
    }
    return true;
}

void DeliverApiClient::DeliverToUserAsync(const std::string& gateway_addr,
                                           const im::DeliverToUserReq& req) {
    // Fire-and-forget: spawn a detached thread to deliver
    // Message is already persisted, so we don't need to wait for response
    std::thread([this, gateway_addr, req]() {
        auto* pool = GetOrCreatePool(gateway_addr);
        auto stub = pool->getStub();
        if (!stub) {
            LOG_DEBUG("DeliverApiClient::DeliverToUserAsync: no stub for {}", gateway_addr);
            return;
        }

        grpc::ClientContext ctx;
        ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(100));

        im::DeliverToUserResp resp;
        auto status = stub->DeliverToUser(&ctx, req, &resp);
        pool->returnStub(std::move(stub));

        if (!status.ok()) {
            LOG_DEBUG("DeliverApiClient::DeliverToUserAsync: failed for gateway={}: {}",
                      gateway_addr, status.error_message());
        }
        // Whether success or fail, we don't care - message is already in offline queue
    }).detach();
}

GrpcStubPool<im::DeliverApi>* DeliverApiClient::GetOrCreatePool(const std::string& addr) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = pools_.find(addr);
    if (it == pools_.end()) {
        pools_[addr] = std::make_unique<GrpcStubPool<im::DeliverApi>>(addr, 4);
        return pools_[addr].get();
    }
    return it->second.get();
}
