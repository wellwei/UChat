#include "StatusGrpcClient.h"
#include "ConfigMgr.h"
#include "proto_generated/StatusServer.grpc.pb.h"

StatusGrpcClient::StatusGrpcClient() {
    auto &config_mgr = ConfigMgr::getInstance();
    const std::string host = config_mgr["StatusServer"].get("host", "127.0.0.1");
    const std::string port = config_mgr["StatusServer"].get("port", "80");

    stubs_ = std::make_unique<GrpcStubPool<StatusService>>(host + ":" + port, 10);
}

GetChatServerResponse StatusGrpcClient::GetChatServer(const uint64_t &uid) {
    GetChatServerRequest request;
    GetChatServerResponse reply;
    grpc::ClientContext context;
    request.set_uid(uid);
    auto stub = stubs_->getStub();
    grpc::Status status = stub->GetChatServer(&context, request, &reply);
    stubs_->returnStub(std::move(stub));
    if (!status.ok()) {
        LOG_ERROR("GetChatServer failed: {}", status.error_message());
    }
    return reply;
}

RegisterChatServerResponse StatusGrpcClient::RegisterChatServer(const std::string &server_id, const std::string &host, const std::string &port) {
    RegisterChatServerRequest request;
    RegisterChatServerResponse reply;
    grpc::ClientContext context;
    request.set_server_id(server_id);
    request.set_host(host);
    request.set_port(port);
    auto stub = stubs_->getStub();
    grpc::Status status = stub->RegisterChatServer(&context, request, &reply);
    stubs_->returnStub(std::move(stub));
    if (!status.ok()) {
        LOG_ERROR("RegisterChatServer failed: {}", status.error_message());
    }
    return reply;
}

UnregisterChatServerResponse StatusGrpcClient::UnregisterChatServer(const std::string &server_id) {
    UnregisterChatServerRequest request;
    UnregisterChatServerResponse reply;
    grpc::ClientContext context;
    request.set_server_id(server_id);
    auto stub = stubs_->getStub();
    grpc::Status status = stub->UnregisterChatServer(&context, request, &reply);
    stubs_->returnStub(std::move(stub));
    if (!status.ok()) {
        LOG_ERROR("UnregisterChatServer failed: {}", status.error_message());
    }
    return reply;
}

ChatServerHeartbeatResponse StatusGrpcClient::ChatServerHeartbeat(const std::string &server_id, const uint64_t &current_connections) {
    ChatServerHeartbeatRequest request;
    ChatServerHeartbeatResponse reply;
    grpc::ClientContext context;
    request.set_server_id(server_id);
    request.set_current_connections(current_connections);
    auto stub = stubs_->getStub();
    grpc::Status status = stub->ChatServerHeartbeat(&context, request, &reply);
    stubs_->returnStub(std::move(stub));
    if (!status.ok()) {
        LOG_ERROR("ChatServerHeartbeat failed: {}", status.error_message());
    }
    return reply;
}

UserOnlineStatusUpdateResponse StatusGrpcClient::UserOnlineStatusUpdate(const uint64_t &uid, const bool &online) {
    UserOnlineStatusUpdateRequest request;
    UserOnlineStatusUpdateResponse reply;
    grpc::ClientContext context;
    request.set_uid(uid);
    request.set_online(online);
    auto stub = stubs_->getStub();
    grpc::Status status = stub->UserOnlineStatusUpdate(&context, request, &reply);
    stubs_->returnStub(std::move(stub));
    if (!status.ok()) {
        LOG_ERROR("UserOnlineStatusUpdate failed: {}", status.error_message());
    }
    return reply;
}

GetUidChatServerResponse StatusGrpcClient::GetUidChatServer(const uint64_t &uid) {
    GetUidChatServerRequest request;
    GetUidChatServerResponse reply;
    grpc::ClientContext context;
    request.set_uid(uid);
    auto stub = stubs_->getStub();
    grpc::Status status = stub->GetUidChatServer(&context, request, &reply);
    stubs_->returnStub(std::move(stub));
    if (!status.ok()) {
        LOG_ERROR("GetUidChatServer failed: {}", status.error_message());
    }
    return reply;
}
