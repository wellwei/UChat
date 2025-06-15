#include "MQGatewayClient.h"
#include "StatusGrpcClient.h"
#include "Logger.h"
#include "mqgateway.grpc.pb.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>

using json = nlohmann::json;

MQGatewayClient::MQGatewayClient() 
    : m_connected(false)
    , m_subscribeRunning(false) {
}

MQGatewayClient::~MQGatewayClient() {
    shutdown();
}

bool MQGatewayClient::initialize(const std::string& serverAddress) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_serverAddress = serverAddress;
    
    // 创建gRPC通道
    m_channel = grpc::CreateChannel(serverAddress, grpc::InsecureChannelCredentials());
    if (!m_channel) {
        LOG_ERROR("创建gRPC通道失败: {}", serverAddress);
        return false;
    }
    
    // 创建stub
    m_stub = mqgateway::MQGatewayService::NewStub(m_channel);
    if (!m_stub) {
        LOG_ERROR("创建gRPC stub失败");
        return false;
    }
    
    // 等待连接建立
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    if (!m_channel->WaitForConnected(deadline)) {
        LOG_ERROR("连接MQGateway超时: {}", serverAddress);
        return false;
    }
    
    m_connected = true;
    LOG_INFO("成功连接到MQGateway: {}", serverAddress);
    
    return true;
}

void MQGatewayClient::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_connected = false;
    m_subscribeRunning = false;
    
    if (m_subscribeThread.joinable()) {
        m_subscribeThread.join();
    }
    
    m_stub.reset();
    m_channel.reset();
    
    LOG_INFO("MQGateway客户端已关闭");
}

bool MQGatewayClient::publishMessage(const std::string& routingKey,
                                   MQMessageType messageType,
                                   const std::string& payload,
                                   uint64_t targetUserId,
                                   uint64_t senderUserId,
                                   int64_t timestamp) {
    if (!m_connected || !m_stub) {
        LOG_ERROR("MQGateway客户端未连接");
        return false;
    }
    
    mqgateway::PublishMessageRequest request;
    request.set_routing_key(routingKey);
    request.set_message_type(static_cast<mqgateway::MessageType>(messageType));
    request.set_payload(payload);
    request.set_target_user_id(targetUserId);
    request.set_sender_user_id(senderUserId);
    
    if (timestamp == 0) {
        timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    request.set_timestamp(timestamp);
    
    mqgateway::PublishMessageResponse response;
    grpc::ClientContext context;
    
    // 设置超时
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    context.set_deadline(deadline);
    
    grpc::Status status = m_stub->PublishMessage(&context, request, &response);
    
    if (!status.ok()) {
        LOG_ERROR("发布消息失败: {} - {}", std::to_string(status.error_code()), status.error_message());
        return false;
    }
    
    if (!response.success()) {
        LOG_ERROR("发布消息失败: {}", response.error_message());
        return false;
    }
    
    LOG_DEBUG("消息发布成功: message_id={}, routing_key={}", 
             response.message_id(), routingKey);
    return true;
}

bool MQGatewayClient::publishContactRequestNotification(uint64_t targetUserId,
                                                      uint64_t requesterId,
                                                      uint64_t requestId,
                                                      const std::string& requesterName,
                                                      const std::string& message) {
    json payload;
    payload["type"] = "contact_request";
    payload["request_id"] = requestId;
    payload["requester_id"] = requesterId;
    payload["requester_name"] = requesterName;
    payload["message"] = message;
    payload["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // 查询目标用户所在的 ChatServer
    return publishNotificationToUser(targetUserId, requesterId, MQMessageType::CONTACT_REQUEST, payload.dump());
}

bool MQGatewayClient::publishContactAcceptedNotification(uint64_t targetUserId,
                                                       uint64_t accepterId,
                                                       const std::string& accepterName) {
    json payload;
    payload["type"] = "contact_accepted";
    payload["accepter_id"] = accepterId;
    payload["accepter_name"] = accepterName;
    payload["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // 查询目标用户所在的 ChatServer
    return publishNotificationToUser(targetUserId, accepterId, MQMessageType::CONTACT_ACCEPTED, payload.dump());
}

bool MQGatewayClient::publishContactRejectedNotification(uint64_t targetUserId,
                                                       uint64_t rejecterId,
                                                       const std::string& rejecterName) {
    json payload;
    payload["type"] = "contact_rejected";
    payload["rejecter_id"] = rejecterId;
    payload["rejecter_name"] = rejecterName;
    payload["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // 查询目标用户所在的 ChatServer
    return publishNotificationToUser(targetUserId, rejecterId, MQMessageType::CONTACT_REJECTED, payload.dump());
}

bool MQGatewayClient::subscribe(const std::string& queueName,
                              const std::vector<std::string>& routingKeys,
                              MessageNotificationCallback callback) {
    if (!m_connected || !m_stub) {
        LOG_ERROR("MQGateway客户端未连接");
        return false;
    }
    
    if (m_subscribeRunning) {
        LOG_WARN("订阅已在运行中");
        return false;
    }
    
    m_subscribeRunning = true;
    m_subscribeThread = std::thread(&MQGatewayClient::subscribeWorker, 
                                   this, queueName, routingKeys, callback);
    
    LOG_INFO("开始订阅消息: queue={}, routing_keys={}", queueName, 
             routingKeys.empty() ? "[]" : routingKeys[0]);
    return true;
}

bool MQGatewayClient::healthCheck() {
    if (!m_connected || !m_stub) {
        return false;
    }
    
    mqgateway::HealthCheckRequest request;
    mqgateway::HealthCheckResponse response;
    grpc::ClientContext context;
    
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(3);
    context.set_deadline(deadline);
    
    grpc::Status status = m_stub->HealthCheck(&context, request, &response);
    
    if (!status.ok()) {
        LOG_ERROR("健康检查失败: {} - {}", std::to_string(status.error_code()), status.error_message());
        return false;
    }
    
    return response.healthy();
}

bool MQGatewayClient::isConnected() const {
    return m_connected;
}

bool MQGatewayClient::reconnect() {
    LOG_INFO("尝试重连MQGateway...");
    
    shutdown();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    return initialize(m_serverAddress);
}

// 订阅线程函数
void MQGatewayClient::subscribeWorker(const std::string& queueName,
                                    const std::vector<std::string>& routingKeys,
                                    const MessageNotificationCallback &callback) {
    while (m_subscribeRunning) {
        try {
            if (!m_connected || !m_stub) {
                LOG_WARN("MQGateway连接断开，尝试重连...");
                if (!reconnect()) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    continue;
                }
            }
            
            mqgateway::SubscribeRequest request;
            request.set_queue_name(queueName);
            for (const auto& key : routingKeys) {
                request.add_routing_keys(key);
            }
            
            grpc::ClientContext context;
            auto stream = m_stub->Subscribe(&context, request);
            
            mqgateway::MessageNotification notification;
            while (m_subscribeRunning && stream->Read(&notification)) {
                // 转换消息类型
                MQMessageType msgType = static_cast<MQMessageType>(notification.message_type());
                
                // 调用回调函数
                if (callback) {
                    callback(notification.message_id(),
                           notification.routing_key(),
                           msgType,
                           notification.payload(),
                           notification.target_user_id(),
                           notification.sender_user_id(),
                           notification.timestamp());
                }
            }
            
            grpc::Status status = stream->Finish();
            if (!status.ok() && m_subscribeRunning) {
                LOG_ERROR("订阅流断开: {} - {}", std::to_string(status.error_code()), status.error_message());
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("订阅工作线程异常: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    LOG_INFO("订阅工作线程已退出");
}

bool MQGatewayClient::publishNotificationToUser(uint64_t targetUserId, uint64_t senderUserId, 
                                               MQMessageType messageType, const std::string& payload) {
    try {
        // 查询目标用户所在的 ChatServer
        auto statusClient = StatusGrpcClient::getInstance();
        auto reply = statusClient->GetUidChatServer(targetUserId);
        
        if (reply.code() == GetUidChatServerResponse_Code_OK) {
            // 用户在线，发送通知到对应的 ChatServer 通知队列
            std::string routingKey = reply.notification_queue_name();
            
            LOG_DEBUG("发送通知到用户 {} 所在的 ChatServer 通知队列: {}", targetUserId, routingKey);
            
            return publishMessage(routingKey, messageType, payload, targetUserId, senderUserId);
        } else if (reply.code() == GetUidChatServerResponse_Code_USER_OFFLINE) {
            // 用户离线，可以存储离线通知或直接忽略
            LOG_INFO("用户 {} 当前离线，通知暂时无法发送", targetUserId);
            // TODO: 可以考虑存储离线通知
            return true; // 返回 true 表示处理完成，虽然用户离线
        } else {
            LOG_ERROR("查询用户 {} 所在 ChatServer 失败: {}", targetUserId, reply.error_msg());
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("发送通知到用户 {} 异常: {}", targetUserId, e.what());
        return false;
    }
} 