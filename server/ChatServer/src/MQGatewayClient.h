#pragma once

#include "Singleton.h"
#include <grpcpp/grpcpp.h>
#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include "mqgateway.grpc.pb.h"

// 前向声明
namespace mqgateway {
    class MQGatewayService;
    class PublishMessageRequest;
    class PublishMessageResponse;
    class SubscribeRequest;
    class MessageNotification;
}

class MQGatewayClient : public Singleton<MQGatewayClient> {
    friend class Singleton<MQGatewayClient>;

public:
    ~MQGatewayClient();

    // 初始化连接
    bool initialize(const std::string& server_address);
    
    // 关闭连接
    void shutdown();
    
    // 检查连接状态
    bool isConnected() const;
    
    // 发布消息到指定队列
    bool publishMessage(const std::string& routing_key, 
                       const std::string& message_type,
                       uint64_t target_user_id,
                       uint64_t sender_user_id,
                       const std::string& payload) const;
    
    // 订阅消息队列
    bool subscribeToQueue(const std::string& queue_name, 
                         const std::vector<std::string>& routing_keys,
                         const std::function<void(const std::string&, const std::string&, uint64_t, uint64_t, const std::string&)> &callback);
    
    // 停止订阅
    void stopSubscription();

private:
    MQGatewayClient();
    
    // 订阅处理线程
    void subscriptionThread();
    
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<mqgateway::MQGatewayService::Stub> stub_;
    
    std::atomic<bool> connected_;
    std::atomic<bool> should_stop_;
    std::thread subscription_thread_;
    
    std::function<void(const std::string&, const std::string&, uint64_t, uint64_t, const std::string&)> message_callback_;
    std::string queue_name_;
    std::vector<std::string> routing_keys_;
}; 