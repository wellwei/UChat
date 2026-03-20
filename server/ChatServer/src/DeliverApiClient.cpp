#include "DeliverApiClient.h"
#include "Logger.h"
#include <chrono>
#include <thread>

bool DeliverApiClient::DeliverToUser(const std::string& gateway_addr,
                                     const im::DeliverToUserReq& req) {
    const auto stub = im::DeliverApi::NewStub(channel_pool_.GetChannel(gateway_addr));
    if (!stub) {
        LOG_ERROR("DeliverApiClient: no stub available for {}", gateway_addr);
        return false;
    }

    grpc::ClientContext ctx;
    ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(50));
    im::DeliverToUserResp resp;

    const auto status = stub->DeliverToUser(&ctx, req, &resp);

    if (!status.ok()) {
        LOG_WARN("DeliverApiClient: RPC failed for gateway={}: {}", gateway_addr, status.error_message());
        return false;
    }
    return true;
}