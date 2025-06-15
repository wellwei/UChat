#include "StatusGrpcClient.h"
#include "ConfigMgr.h"
#include "Logger.h"
#include <chrono>

StatusGrpcClient::StatusGrpcClient() {
    auto config_mgr = ConfigMgr::getInstance();
    const std::string host = (*config_mgr)["StatusServer"]["host"].empty() ? "127.0.0.1" : (*config_mgr)["StatusServer"]["host"];
    const std::string port = (*config_mgr)["StatusServer"]["port"].empty() ? "50051" : (*config_mgr)["StatusServer"]["port"];
    
    server_address_ = host + ":" + port;
    
    // 创建 gRPC 通道
    channel_ = grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials());
    stub_ = StatusService::NewStub(channel_);
    
    LOG_INFO("StatusGrpcClient 初始化完成，服务器地址: {}", server_address_);
}

GetUidChatServerResponse StatusGrpcClient::GetUidChatServer(const uint64_t &uid) {
    GetUidChatServerRequest request;
    GetUidChatServerResponse reply;
    grpc::ClientContext context;
    
    // 设置超时
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    context.set_deadline(deadline);
    
    request.set_uid(uid);
    
    grpc::Status status = stub_->GetUidChatServer(&context, request, &reply);
    
    if (!status.ok()) {
        LOG_ERROR("GetUidChatServer failed: {} - {}", static_cast<int>(status.error_code()), status.error_message());
        reply.set_code(GetUidChatServerResponse_Code_INTERNAL_ERROR);
        reply.set_error_msg("gRPC调用失败: " + status.error_message());
    }
    
    return reply;
}

bool StatusGrpcClient::isConnected() {
    if (!channel_) {
        return false;
    }
    
    auto state = channel_->GetState(false);
    return state == GRPC_CHANNEL_READY || state == GRPC_CHANNEL_IDLE;
} 