#include "MQGatewayClient.h"
#include "mqgateway.grpc.pb.h"
#include "Logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

MQGatewayClient::MQGatewayClient() 
    : connected_(false), should_stop_(false) {
}

MQGatewayClient::~MQGatewayClient() {
    shutdown();
}

bool MQGatewayClient::initialize(const std::string& server_address) {
    try {
        // 创建 gRPC 通道
        channel_ = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
        stub_ = mqgateway::MQGatewayService::NewStub(channel_);
        
        // 测试连接
        ClientContext context;

        // 设置预期时间
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

        const mqgateway::HealthCheckRequest request;
        mqgateway::HealthCheckResponse response;

        if (const Status status = stub_->HealthCheck(&context, request, &response); status.ok() && response.healthy()) {
            connected_ = true;
            LOG_INFO("MQGateway 客户端连接成功: {}", server_address);
            return true;
        } else {
            LOG_CRITICAL("MQGateway 健康检查失败: {}", status.error_message());
            return false;
        }
    } catch (const std::exception& e) {
        LOG_CRITICAL("初始化 MQGateway 客户端失败: {}", e.what());
        return false;
    }
}

void MQGatewayClient::shutdown() {
    should_stop_ = true;
    connected_ = false;
    
    if (subscription_thread_.joinable()) {
        subscription_thread_.join();
    }
    
    LOG_INFO("MQGateway 客户端已关闭");
}

bool MQGatewayClient::isConnected() const {
    return connected_;
}

/**
 * 发布消息
 * @param routing_key 路由键
 * @param message_type 消息类型
 * @param target_user_id 目标用户ID
 * @param sender_user_id 发送用户ID
 * @param payload 消息内容（JSON）
 * @return
 */
bool MQGatewayClient::publishMessage(const std::string& routing_key, 
                                   const std::string& message_type,
                                   const uint64_t target_user_id,
                                   const uint64_t sender_user_id,
                                   const std::string& payload) const {
    if (!connected_) {
        LOG_ERROR("MQGateway 客户端未连接");
        return false;
    }
    
    try {
        ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(10));
        
        mqgateway::PublishMessageRequest request;
        request.set_routing_key(routing_key);
        request.set_target_user_id(target_user_id);
        request.set_sender_user_id(sender_user_id);
        request.set_payload(payload);   // 消息内容（JSON）
        request.set_timestamp(std::time(nullptr));
        
        // 设置消息类型
        if (message_type == "chat_message") {
            request.set_message_type(mqgateway::MessageType::CHAT_MESSAGE);
        } else if (message_type == "contact_request") {
            request.set_message_type(mqgateway::MessageType::CONTACT_REQUEST);
        } else if (message_type == "contact_accepted") {
            request.set_message_type(mqgateway::MessageType::CONTACT_ACCEPTED);
        } else if (message_type == "contact_rejected") {
            request.set_message_type(mqgateway::MessageType::CONTACT_REJECTED);
        } else if (message_type == "notification") {
            request.set_message_type(mqgateway::MessageType::SYSTEM_NOTIFICATION);
        } else {
            request.set_message_type(mqgateway::MessageType::CHAT_MESSAGE); // 默认
        }
        
        mqgateway::PublishMessageResponse response;

        if (const Status status = stub_->PublishMessage(&context, request, &response); status.ok() && response.success()) {
            LOG_DEBUG("消息发布成功: routing_key={}, message_id={}", routing_key, response.message_id());
            return true;
        } else {
            LOG_ERROR("消息发布失败: {}", response.error_message());
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("发布消息异常: {}", e.what());
        return false;
    }
}

/**
 * 订阅队列
 * @param queue_name 队列名
 * @param routing_keys 路由键
 * @param callback 回调函数
 * @return
 */
bool MQGatewayClient::subscribeToQueue(const std::string& queue_name,
                                       const std::vector<std::string>& routing_keys,
                                       const std::function<void(const std::string&, const std::string&, uint64_t, uint64_t, const std::string&)> &callback) {
    if (!connected_) {
        LOG_ERROR("MQGateway 客户端未连接");
        return false;
    }
    
    queue_name_ = queue_name;
    routing_keys_ = routing_keys;
    message_callback_ = callback;
    
    // 启动订阅线程
    should_stop_ = false;
    subscription_thread_ = std::thread(&MQGatewayClient::subscriptionThread, this);
    
    LOG_INFO("开始订阅队列: {}", queue_name);
    return true;
}

/**
 * 停止订阅
 */
void MQGatewayClient::stopSubscription() {
    should_stop_ = true;
    if (subscription_thread_.joinable()) {
        subscription_thread_.join();
    }
    LOG_INFO("停止订阅队列: {}", queue_name_);
}

/**
 * 订阅线程
 */
void MQGatewayClient::subscriptionThread() {
    while (!should_stop_) {
        try {
            ClientContext context;
            
            mqgateway::SubscribeRequest request;
            request.set_queue_name(queue_name_);
            for (const auto& routing_key : routing_keys_) {
                request.add_routing_keys(routing_key);
            }
            
            auto stream = stub_->Subscribe(&context, request);
            
            mqgateway::MessageNotification notification;
            while (!should_stop_ && stream->Read(&notification)) {
                try {
                    // 解析消息内容
                    json payload_json = json::parse(notification.payload());
                    
                    std::string message_type;
                    switch (notification.message_type()) {
                        case mqgateway::MessageType::CHAT_MESSAGE:
                            message_type = "chat_message";
                            break;
                        case mqgateway::MessageType::CONTACT_REQUEST:
                            message_type = "contact_request";
                            break;
                        case mqgateway::MessageType::CONTACT_ACCEPTED:
                            message_type = "contact_accepted";
                            break;
                        case mqgateway::MessageType::CONTACT_REJECTED:
                            message_type = "contact_rejected";
                            break;
                        case mqgateway::MessageType::SYSTEM_NOTIFICATION:
                            message_type = "notification";
                            break;
                        default:
                            message_type = "unknown";
                            break;
                    }
                    
                    // 调用回调函数
                    if (message_callback_) {
                        message_callback_(
                            notification.routing_key(),
                            message_type,
                            notification.target_user_id(),
                            notification.sender_user_id(),
                            notification.payload()
                        );
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("处理消息通知异常: {}", e.what());
                }
            }
            
            Status status = stream->Finish();
            if (!status.ok() && !should_stop_) {
                LOG_ERROR("订阅流异常: {}", status.error_message());
                std::this_thread::sleep_for(std::chrono::seconds(5)); // 重连延迟
            }
        } catch (const std::exception& e) {
            LOG_ERROR("订阅线程异常: {}", e.what());
            if (!should_stop_) {
                std::this_thread::sleep_for(std::chrono::seconds(5)); // 重连延迟
            }
        }
    }
} 