#pragma once

#include "GrpcStubPool.h"
#include "Logger.h"
#include "Singleton.h"
#include "chatserver.grpc.pb.h"
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>

using grpc::Status;
using grpc::ClientContext;
using grpc::Channel;

class InterChatClient : public Singleton<InterChatClient> {
    friend class Singleton<InterChatClient>;

public:
    // 转发消息到其他聊天服务器
    bool ForwardMessage(
        const std::string& target_host, 
        const std::string& target_port,
        const std::string& message_id,
        const std::string& sender_id,
        const std::string& recipient_id,
        const std::string& content
    );

private:
    InterChatClient() = default;
    
    // 获取连接到指定服务器的stub
    std::shared_ptr<InterChatService::Stub> getStub(const std::string& target_host, const std::string& target_port);

    // 缓存创建的stub池
    std::unordered_map<std::string, std::unique_ptr<GrpcStubPool<InterChatService>>> stub_pools_;
    std::mutex stub_pools_mutex_;
}; 