//
// Created by wellwei on 2025/4/4.
//

#include "VerifyGrpcClient.h"
#include "ConfigMgr.h"
#include "Logger.h"

VerifyGrpcClient::VerifyGrpcClient() {
    auto config_mgr = *ConfigMgr::getInstance(); // 获取配置管理器实例
    std::string server_address = config_mgr["VerifyServer"]["host"] + ":" + config_mgr["VerifyServer"]["port"];
    stub_pool_ = std::make_unique<GrpcStubPool<VerifyService>>(8, server_address); // 创建 gRPC 存根池
}

VerifyResponse VerifyGrpcClient::getVerifyCode(const std::string &email) {
    VerifyRequest request;         // 创建请求对象
    VerifyResponse reply;          // 创建响应对象
    ClientContext context;          // 创建上下文对象

    request.set_email(email);
    auto stub = stub_pool_->getStub(); // 从存根池获取存根
    Status status = stub->GetVerifyCode(&context, request, &reply); // 发送请求并接收响应
    stub_pool_->returnStub(std::move(stub)); // 将存根放回池中
    if (status.ok()) {
        return reply; // 返回响应对象
    } else {
        reply.set_code(ErrorCode::RPC_ERROR);
        LOG_ERROR("RPC faild: {}", status.error_message());
        return reply; // 返回错误响应
    }
}
