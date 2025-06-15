#include <boost/bind.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

#include "ChatServer.h"
#include "ConfigMgr.h"
#include "Logger.h"
#include "StatusGrpcClient.h"
#include "ServicePool.h"

using json = nlohmann::json;

ChatServer::ChatServer(asio::io_context &io_context, const uint16_t port)
    : io_context_(io_context),
      acceptor_(io_context_, asio::ip::tcp::endpoint(asio::ip::address_v4::any(), port)),
      heartbeat_timer_(io_context) {
    auto &config = ConfigMgr::getInstance();

    // 获取服务器配置
    host_ = config["ChatServer"].get("host", "127.0.0.1");
    port_ = config["ChatServer"].get("port", "8088");
    heartbeat_interval_ = std::stoi(config["ChatServer"].get("heartbeat_interval", "10"));
    connect_timeout_ = std::stoi(config["ChatServer"].get("connect_timeout", "30"));

    // 获取服务器ID和队列名称
    server_id_ = config["ChatServer"].get("server_id", "");
    if (server_id_.empty()) {
        server_id_ = boost::uuids::to_string(boost::uuids::random_generator()());
    }
    
    message_queue_name_ = config["ChatServer"].get("message_queue_name", server_id_ + "_messages");
    notification_queue_name_ = config["ChatServer"].get("notification_queue_name", server_id_ + "_notifications");

    // Get JWT secret key
    std::string jwt_secret_key = config["JWT"].get("secret_key", "");
    if (jwt_secret_key.empty()) {
        throw std::runtime_error("未配置 JWT secret_key");
    }

    // 初始化消息处理器
    message_handler_ = std::make_unique<MessageHandler>(connection_manager_, jwt_secret_key, server_id_);

    LOG_INFO("ChatServer 初始化完成, ID: {}, 监听 {}:{}, 消息队列: {}, 通知队列: {}",
             server_id_, host_, port_, message_queue_name_, notification_queue_name_);
}

ChatServer::~ChatServer() {
    stop();
}

void ChatServer::start() {
    LOG_INFO("开始启动 ChatServer...");

    // 初始化MQGateway客户端
    init_mq_gateway();

    // 注册服务器到状态服务器
    register_server();

    // 开始接受连接
    do_accept();

    // 开始心跳计时
    send_heartbeat();

    LOG_INFO("ChatServer 启动完成");
}

void ChatServer::stop() {
    LOG_INFO("开始停止 ChatServer...");

    // 关闭MQGateway客户端
    MQGatewayClient::getInstance().shutdown();

    // 关闭所有连接
    connection_manager_.closeAllConnections();

    // 停止心跳计时
    heartbeat_timer_.cancel();

    // 从状态服务器注销
    try {
        if (const auto response = StatusGrpcClient::getInstance().UnregisterChatServer(server_id_);
            response.code() == UnregisterChatServerResponse_Code_OK) {
            LOG_INFO("从状态服务器注销成功");
        } else {
            LOG_ERROR("从状态服务器注销失败: {}", response.error_msg());
        }
    } catch (const std::exception &e) {
        LOG_ERROR("从状态服务器注销失败: {}", e.what());
    }

    LOG_INFO("ChatServer 停止完成");
}

void ChatServer::do_accept() {
    // 创建一个新的连接
    auto conn = std::make_shared<TcpConnection>(ServicePool::getInstance().getNextService(), connect_timeout_, server_id_);

    // 接受新的连接
    acceptor_.async_accept(conn->socket(),
        std::bind(&ChatServer::handle_accept, shared_from_this(), conn, std::placeholders::_1));
}

void ChatServer::handle_accept(const TcpConnectionPtr &conn, const boost::system::error_code& error) {
    if (!error) {
        LOG_INFO("接受新的连接: {}", conn->id());

        // 设置回调
        conn->setMessageCallback(std::bind(&ChatServer::handle_message, shared_from_this(),
                                          std::placeholders::_1, std::placeholders::_2));
        conn->setCloseCallback(std::bind(&ChatServer::handle_close, shared_from_this(),
                                        std::placeholders::_1));

        // 添加到连接管理器
        connection_manager_.addConnection(conn);

        // 开始从连接读取
        conn->start();
    } else {
        LOG_ERROR("接受错误: {}", error.message());
    }

    // 不管有没有发生错误都继续接收其他连接
    do_accept();
}

void ChatServer::handle_message(const TcpConnectionPtr& conn, const Message& msg) const {
    // 委托消息处理到消息处理器
    message_handler_->handleMessage(conn, msg);
}

void ChatServer::handle_close(const TcpConnectionPtr& conn) {
    LOG_INFO("连接关闭: {}", conn->id());

    // 如果是已认证的用户，更新用户在线状态
    if (conn->isAuthenticated() && conn->uid() > 0) {
        try {
            if (const auto reply = StatusGrpcClient::getInstance().UserOnlineStatusUpdate(conn->uid(), server_id_, false);
                reply.code() != UserOnlineStatusUpdateResponse_Code_OK) {
                LOG_ERROR("更新用户 {} 离线状态失败: {}", conn->uid(), reply.error_msg());
            } else {
                LOG_INFO("用户 {} 从服务器 {} 离线", conn->uid(), server_id_);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("更新用户离线状态异常: {}", e.what());
        }
    }

    // 从连接管理器中移除
    connection_manager_.removeConnection(conn->id());
}

void ChatServer::register_server() {
    try {
        LOG_INFO("开始向状态服务器注册, server_id: {}, host: {}, port: {}", server_id_, host_, port_);
        
        // 注册到状态服务器
        const auto response = StatusGrpcClient::getInstance().RegisterChatServer(
            server_id_, host_, port_, message_queue_name_, notification_queue_name_);

        if (response.code() == RegisterChatServerResponse_Code_OK) {
            LOG_INFO("注册到状态服务器成功");
        } else {
            LOG_ERROR("注册到状态服务器失败: {}", response.error_msg());

            // 如果是因为ID已存在，重新生成ID
            if (response.code() == RegisterChatServerResponse_Code_ALREADY_REGISTERED) {
                LOG_INFO("Server ID 已存在，重新生成");
                server_id_ = boost::uuids::to_string(boost::uuids::random_generator()());
            }

            // 稍后重试
            auto self = shared_from_this();
            auto timer = std::make_shared<asio::steady_timer>(io_context_, std::chrono::seconds(5));
            timer->async_wait([this, self, timer](const boost::system::error_code&) {
                register_server();
            });
        }
    } catch (const std::exception& e) {
        LOG_ERROR("注册到状态服务器失败: {}", e.what());

        // 稍后重试
        auto self = shared_from_this();
        auto timer = std::make_shared<asio::steady_timer>(io_context_, std::chrono::seconds(5));
        timer->async_wait([this, self, timer](const boost::system::error_code&) {
            register_server();
        });
    }
}

void ChatServer::send_heartbeat() {
    try {
        // 发送心跳到状态服务器
        uint64_t conn_count = connection_manager_.connectionCount();
        LOG_DEBUG("发送心跳, server_id: {}, 当前连接数: {}", server_id_, conn_count);

        if (const auto response = StatusGrpcClient::getInstance().ChatServerHeartbeat(server_id_, conn_count);
            response.code() != ChatServerHeartbeatResponse_Code_OK) {
            LOG_WARN("心跳失败: {}, 错误码: {}", response.error_msg(), static_cast<int>(response.code()));

            // 如果服务器未知，尝试重新注册
            if (response.code() == ChatServerHeartbeatResponse_Code_UNKNOWN_SERVER) {
                LOG_INFO("服务器ID未知，重新注册到状态服务器");
                register_server();
            }
        } else {
            LOG_DEBUG("心跳发送成功");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("发送心跳失败: {}", e.what());
    }

    // 安排下一个心跳
    auto self = shared_from_this();
    heartbeat_timer_.expires_after(std::chrono::seconds(heartbeat_interval_));
    heartbeat_timer_.async_wait([this, self](const boost::system::error_code& error) {
        if (!error) {
            send_heartbeat();
        }
    });
}

void ChatServer::init_mq_gateway() {
    try {
        auto &config = ConfigMgr::getInstance();
        std::string mq_host = config["MQGateway"].get("host", "localhost");
        std::string mq_port = config["MQGateway"].get("port", "50056");
        std::string mq_address = mq_host + ":" + mq_port;

        if (MQGatewayClient::getInstance().initialize(mq_address)) {
            LOG_INFO("MQGateway 客户端初始化成功: {}", mq_address);
            
            // 订阅消息队列
            std::vector<std::string> routing_keys = {message_queue_name_, notification_queue_name_};
            MQGatewayClient::getInstance().subscribeToQueue(
                message_queue_name_,
                routing_keys,
                std::bind(&ChatServer::handle_mq_message, this, 
                         std::placeholders::_1, std::placeholders::_2, 
                         std::placeholders::_3, std::placeholders::_4, 
                         std::placeholders::_5)
            );
            
            LOG_INFO("开始订阅消息队列: {}", message_queue_name_);
        } else {
            LOG_CRITICAL("MQGateway 客户端初始化失败: {}", mq_address);
            exit(1);
        }
    } catch (const std::exception& e) {
        LOG_CRITICAL("初始化MQGateway失败: {}", e.what());
        exit(1);
    }
}

void ChatServer::handle_mq_message(const std::string& routing_key, const std::string& message_type, 
                                  uint64_t target_user_id, uint64_t sender_user_id, const std::string& payload) {
    try {
        LOG_DEBUG("收到MQ消息: routing_key={}, type={}, target={}, sender={}", 
                 routing_key, message_type, target_user_id, sender_user_id);
        
        if (message_type == "chat_message") {
            // 处理聊天消息
            json payload_json = json::parse(payload);
            
            // 查找目标用户连接
            auto to_conn = connection_manager_.findConnectionByUid(target_user_id);
            if (to_conn) {
                // 构造接收消息，格式与 MessageHandler 保持一致
                json message_json;
                message_json["type"] = MessageType::RecvMsg;
                
                json message_data;
                // 确保 from_uid 是字符串格式，与客户端期望一致
                message_data["from_uid"] = std::to_string(sender_user_id);
                message_data["msg_type"] = payload_json.value("msg_type", "text");
                message_data["content"] = payload_json.value("content", "");
                // 确保 timestamp 是数字格式
                message_data["timestamp"] = payload_json.value("timestamp", static_cast<uint64_t>(std::time(nullptr)));
                
                message_json["data"] = message_data;
                
                // 创建要发送的消息
                Message forward_msg;
                forward_msg.msg_id = 0; // 系统消息，不需要回复
                forward_msg.content = message_json.dump();
                forward_msg.content_len = static_cast<uint16_t>(forward_msg.content.length());
                
                // 发送消息给目标用户
                to_conn->sendMessage(forward_msg);
                
                LOG_INFO("MQ消息已转发给用户 {} (来自用户 {})", target_user_id, sender_user_id);
            } else {
                LOG_WARN("目标用户 {} 不在本服务器上", target_user_id);
            }
        } else if (message_type == "contact_request" || 
                   message_type == "contact_accepted" || 
                   message_type == "contact_rejected" || 
                   message_type == "notification") {
            // 处理联系人请求和通知
            if (auto to_conn = connection_manager_.findConnectionByUid(target_user_id)) {
                // 构造通知消息，使用统一的消息格式
                json notification_json;
                notification_json["type"] = "notification";
                
                json notification_data;
                notification_data["notification_type"] = message_type;
                notification_data["sender_user_id"] = std::to_string(sender_user_id);
                notification_data["target_user_id"] = std::to_string(target_user_id);
                notification_data["timestamp"] = static_cast<uint64_t>(std::time(nullptr));
                
                // 解析 payload 并合并到 data 中
                try {
                    json payload_json = json::parse(payload);
                    for (auto& [key, value] : payload_json.items()) {
                        notification_data[key] = value;
                    }
                } catch (const std::exception& e) {
                    LOG_WARN("解析通知 payload 失败: {}", e.what());
                    notification_data["payload"] = payload;
                }
                
                notification_json["data"] = notification_data;
                
                // 创建要发送的消息
                Message notification_msg;
                notification_msg.msg_id = 0; // 系统消息，不需要回复
                notification_msg.content = notification_json.dump();
                notification_msg.content_len = static_cast<uint16_t>(notification_msg.content.length());
                
                // 发送通知给目标用户
                to_conn->sendMessage(notification_msg);
                
                LOG_INFO("通知已发送给用户 {} (来自用户 {}): {}", target_user_id, sender_user_id, message_type);
            } else {
                LOG_WARN("目标用户 {} 不在本服务器上", target_user_id);
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("处理MQ消息异常: {}", e.what());
    }
}