//
// Created by wellwei on 25-4-17.
//

#include "StatusGrpcClient.h"
#include "ConfigMgr.h"
#include "Logger.h"

StatusGrpcClient::StatusGrpcClient() {
    auto cfm = *ConfigMgr::getInstance();
    std::string server_address = cfm["StatusServer"]["host"] + ":" + cfm["StatusServer"]["port"];
    LOG_INFO("address : {}", server_address);
    _stub = std::make_unique<GrpcStubPool<StatusService>>(8, server_address);
}

StatusGrpcClient::~StatusGrpcClient() {
    _stub->close();
}

GetChatServerResponse StatusGrpcClient::getChatServer(const uint64_t &uid) {
    GetChatServerRequest request;
    GetChatServerResponse reply;
    ClientContext context;

    request.set_uid(uid);
    auto stub = _stub->getStub();
    const Status status = stub->GetChatServer(&context, request, &reply);
    _stub->returnStub(std::move(stub));
    if (status.ok()) {
        return reply;
    } else {
        LOG_ERROR("RPC failed: {}", status.error_message());
        return reply;
    }
}



