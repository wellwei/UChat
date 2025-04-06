//
// Created by wellwei on 2025/4/4.
//

#include "CaptchaGrpcClient.h"

CaptchaResponse CaptchaGrpcClient::getCaptcha(const std::string &email) {
    CaptchaRequest request;         // 创建请求对象
    CaptchaResponse reply;          // 创建响应对象
    ClientContext context;          // 创建上下文对象

    request.set_email(email);
    Status status = stub_->GetCaptcha(&context, request, &reply); // 发送请求并接收响应
    if (status.ok()) {
        return reply; // 返回响应对象
    } else {
        reply.set_code(ErrorCode::RPC_ERROR);
        std::cerr << "RPC failed: " << status.error_message() << std::endl;
        return reply; // 返回错误响应
    }
}

CaptchaGrpcClient::CaptchaGrpcClient() {
    auto config_mgr = *ConfigMgr::getInstance(); // 获取配置管理器实例
    std::string server_address = config_mgr["CaptchaServer"]["host"] + ":" + config_mgr["CaptchaServer"]["port"];
    // 创建 gRPC 通道
    std::shared_ptr<Channel> channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
    stub_ = CaptchaService::NewStub(channel); // 创建 gRPC 客户端存根
}
