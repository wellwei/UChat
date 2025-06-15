#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include <functional>
#include <thread>

#include "Singleton.h"
#include "mqgateway.grpc.pb.h"

// 前向声明
namespace mqgateway {
    class MQGatewayService;
    class PublishMessageRequest;
    class PublishMessageResponse;
    class MessageNotification;
}

// 消息类型枚举
enum class MQMessageType {
    CONTACT_REQUEST = 0,
    CONTACT_ACCEPTED = 1,
    CONTACT_REJECTED = 2,
    CHAT_MESSAGE = 3,
    SYSTEM_NOTIFICATION = 4
};

// 消息通知回调函数类型
using MessageNotificationCallback = std::function<void(const std::string& messageId, 
                                                      const std::string& routingKey,
                                                      MQMessageType messageType,
                                                      const std::string& payload,
                                                      uint64_t targetUserId,
                                                      uint64_t senderUserId,
                                                      int64_t timestamp)>;

class MQGatewayClient : public Singleton<MQGatewayClient> {
    friend class Singleton<MQGatewayClient>;

public:
    ~MQGatewayClient();

    // 初始化连接
    bool initialize(const std::string& serverAddress);
    
    // 关闭连接
    void shutdown();
    
    // 发布单条消息
    bool publishMessage(const std::string& routingKey,
                       MQMessageType messageType,
                       const std::string& payload,
                       uint64_t targetUserId,
                       uint64_t senderUserId = 0,
                       int64_t timestamp = 0);
    
    // 发布联系人请求通知
    bool publishContactRequestNotification(uint64_t targetUserId,
                                         uint64_t requesterId,
                                         uint64_t requestId,
                                         const std::string& requesterName,
                                         const std::string& message);
    
    // 发布联系人接受通知
    bool publishContactAcceptedNotification(uint64_t targetUserId,
                                          uint64_t accepterId,
                                          const std::string& accepterName);
    
    // 发布联系人拒绝通知
    bool publishContactRejectedNotification(uint64_t targetUserId,
                                          uint64_t rejecterId,
                                          const std::string& rejecterName);
    
    // 订阅消息（异步）
    bool subscribe(const std::string& queueName,
                  const std::vector<std::string>& routingKeys,
                  MessageNotificationCallback callback);
    
    // 健康检查
    bool healthCheck();
    
    // 检查连接状态
    bool isConnected() const;

private:
    MQGatewayClient();
    
    // 发送通知到指定用户（查询用户所在的 ChatServer）
    bool publishNotificationToUser(uint64_t targetUserId, uint64_t senderUserId, 
                                  MQMessageType messageType, const std::string& payload);
    
    // 重连逻辑
    bool reconnect();
    
    // 订阅消息处理线程
    void subscribeWorker(const std::string& queueName,
                        const std::vector<std::string>& routingKeys,
                        const MessageNotificationCallback &callback);

private:
    std::shared_ptr<grpc::Channel> m_channel;
    std::unique_ptr<mqgateway::MQGatewayService::Stub> m_stub;
    std::string m_serverAddress;
    bool m_connected;
    std::mutex m_mutex;
    
    // 订阅相关
    std::atomic<bool> m_subscribeRunning;
    std::thread m_subscribeThread;
}; 