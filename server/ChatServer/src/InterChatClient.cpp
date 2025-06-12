#include "InterChatClient.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <chrono>

bool InterChatClient::ForwardMessage(
    const std::string& target_host, 
    const std::string& target_port,
    const std::string& message_id,
    const std::string& sender_id,
    const std::string& recipient_id,
    const std::string& content
) {
    try {
        auto stub = getStub(target_host, target_port);
        if (!stub) {
            LOG_ERROR("获取目标服务器 {}:{} 的stub失败", target_host, target_port);
            return false;
        }

        // 创建转发的消息
        ForwardedMessage request;
        request.set_message_id(message_id);
        request.set_original_sender_id(sender_id);
        request.set_target_recipient_id(recipient_id);
        request.set_content(content);
        
        // 设置当前时间戳
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        request.set_timestamp(std::to_string(now_c));

        // 创建RPC上下文
        ClientContext context;
        
        // 设置超时时间
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
        
        // 创建双向流
        std::shared_ptr<grpc::ClientReaderWriter<ForwardedMessage, ForwardedMessage>> stream = 
            stub->ForwardMessage(&context);
        
        // 发送请求
        if (!stream->Write(request)) {
            LOG_ERROR("向目标服务器 {}:{} 写入消息失败", target_host, target_port);
            return false;
        }
        
        // 关闭写入流
        stream->WritesDone();
        
        // 等待响应
        ForwardedMessage response;
        while (stream->Read(&response)) {
            // 目前我们不处理返回的消息，只记录日志
            LOG_INFO("收到转发消息确认: message_id={}, sender={}, recipient={}", 
                      response.message_id(), response.original_sender_id(), response.target_recipient_id());
        }
        
        // 获取RPC调用状态
        Status status = stream->Finish();
        if (!status.ok()) {
            LOG_ERROR("转发消息到 {}:{} 失败: {}", target_host, target_port, status.error_message());
            return false;
        }
        
        LOG_INFO("消息成功转发到 {}:{}, message_id={}", target_host, target_port, message_id);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("转发消息异常: {}", e.what());
        return false;
    }
}

std::shared_ptr<InterChatService::Stub> InterChatClient::getStub(const std::string& target_host, const std::string& target_port) {
    const std::string target = target_host + ":" + target_port;
    
    std::lock_guard<std::mutex> lock(stub_pools_mutex_);
    
    // 检查是否已经有到该目标的stub池
    auto it = stub_pools_.find(target);
    if (it == stub_pools_.end()) {
        // 创建新的stub池
        auto stub_pool = std::make_unique<GrpcStubPool<InterChatService>>(target, 5);
        stub_pools_[target] = std::move(stub_pool);
        it = stub_pools_.find(target);
    }
    
    // 从池中获取stub
    auto stub = it->second->getStub();
    return stub;
} 