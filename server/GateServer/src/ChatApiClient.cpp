#include "ChatApiClient.h"
#include "ConfigMgr.h"
#include "Logger.h"

ChatApiClient::ChatApiClient() {
    auto& config = *ConfigMgr::getInstance();
    std::string host = config["ChatServer"]["host"];
    std::string port = config["ChatServer"]["port"];
    int pool_size = std::stoi(config["ChatServer"].get("stub_pool_size", "8"));

    std::string addr = host + ":" + port;
    stub_pool_ = std::make_unique<GrpcStubPool<im::ChatApi>>(pool_size, addr);
    LOG_INFO("ChatApiClient initialized, target={}", addr);
}

im::SendMessageResp ChatApiClient::SendMessage(const im::SendMessageReq& req) {
    auto stub = stub_pool_->getStub();
    grpc::ClientContext ctx;
    im::SendMessageResp resp;

    ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

    if (!stub) {
        resp.set_ok(false);
        resp.set_reason("No stub available");
        return resp;
    }
    auto status = stub->SendMessage(&ctx, req, &resp);
    stub_pool_->returnStub(std::move(stub));
    if (!status.ok()) {
        resp.set_ok(false);
        resp.set_reason(status.error_message());
        LOG_ERROR("ChatApiClient::SendMessage failed: {}", status.error_message());
    }
    return resp;
}

im::AckUpResp ChatApiClient::Ack(const im::AckUpReq& req) {
    auto stub = stub_pool_->getStub();
    grpc::ClientContext ctx;
    im::AckUpResp resp;
    if (!stub) return resp;
    stub->Ack(&ctx, req, &resp);
    stub_pool_->returnStub(std::move(stub));
    return resp;
}

im::SyncUpResp ChatApiClient::Sync(const im::SyncUpReq& req) {
    auto stub = stub_pool_->getStub();
    grpc::ClientContext ctx;
    im::SyncUpResp resp;

    ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(5000));

    if (!stub) {
        resp.set_ok(false);
        return resp;
    }
    auto status = stub->Sync(&ctx, req, &resp);
    stub_pool_->returnStub(std::move(stub));
    if (!status.ok()) {
        resp.set_ok(false);
        LOG_ERROR("ChatApiClient::Sync failed: {}", status.error_message());
    }
    return resp;
}
