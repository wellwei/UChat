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
#include "InterChatClient.h"

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
    
    // 获取gRPC端口配置
    grpc_port_ = std::stoi(config["ChatServer"].get("grpc_port", "50056"));

    // 生成 ServerId
    server_id_ = boost::uuids::to_string(boost::uuids::random_generator()());

    // Get JWT secret key
    std::string jwt_secret_key = config["JWT"].get("secret_key", "");
    if (jwt_secret_key.empty()) {
        throw std::runtime_error("未配置 JWT secret_key");
    }

    // 初始化消息处理器
    message_handler_ = std::make_unique<MessageHandler>(connection_manager_, jwt_secret_key);

    LOG_INFO("ChatServer 初始化完成, ID: {}, 监听 {}:{}, gRPC端口: {}", 
             server_id_, host_, port_, grpc_port_);
}

ChatServer::~ChatServer() {
    stop();
}

void ChatServer::start() {
    LOG_INFO("开始启动 ChatServer...");

    // 注册服务器到状态服务器
    register_server();

    // 开始接受连接
    do_accept();

    // 开始心跳计时
    send_heartbeat();
    
    // 启动服务间通信服务
    start_inter_chat_service();

    LOG_INFO("ChatServer 启动完成");
}

void ChatServer::stop() {
    LOG_INFO("开始停止 ChatServer...");

    // 关闭所有连接
    connection_manager_.closeAllConnections();

    // 停止心跳计时
    heartbeat_timer_.cancel();
    
    // 停止gRPC服务
    if (grpc_server_) {
        grpc_server_->Shutdown();
        LOG_INFO("gRPC服务已停止");
    }

    // 从状态服务器注销
    try {
        auto response = StatusGrpcClient::getInstance().UnregisterChatServer(server_id_);
        if (response.code() == UnregisterChatServerResponse_Code_OK) {
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
    auto conn = std::make_shared<TcpConnection>(ServicePool::getInstance().getNextService(), connect_timeout_);

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
    LOG_DEBUG("收到来自连接 {}: {}", conn->id(), msg.content);

    // 委托消息处理到消息处理器
    message_handler_->handleMessage(conn, msg);
}

void ChatServer::handle_close(const TcpConnectionPtr& conn) {
    LOG_INFO("连接关闭: {}", conn->id());

    // 如果是已认证的用户，更新用户在线状态
    if (conn->isAuthenticated() && conn->uid() > 0) {
        try {
            auto reply = StatusGrpcClient::getInstance().UserOnlineStatusUpdate(conn->uid(), false);
            if (reply.code() != UserOnlineStatusUpdateResponse_Code_OK) {
                LOG_ERROR("更新用户 {} 离线状态失败: {}", conn->uid(), reply.error_msg());
            } else {
                LOG_INFO("用户 {} 离线状态已更新", conn->uid());
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
        const auto response = StatusGrpcClient::getInstance().RegisterChatServer(server_id_, host_, port_);

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
        
        auto response = StatusGrpcClient::getInstance().ChatServerHeartbeat(server_id_, conn_count);

        if (response.code() != ChatServerHeartbeatResponse_Code_OK) {
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

void ChatServer::start_inter_chat_service() {
    try {
        LOG_INFO("启动InterChatService，端口: {}", grpc_port_);
        
        // 创建服务实例
        inter_chat_service_ = std::make_unique<InterChatServiceImpl>(connection_manager_, *message_handler_);
        
        // 创建服务构建器
        std::string server_address = "0.0.0.0:" + std::to_string(grpc_port_);
        grpc::ServerBuilder builder;
        
        // 监听地址，不使用凭证
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        
        // 注册服务
        builder.RegisterService(inter_chat_service_.get());
        
        // 创建并启动服务器
        grpc_server_ = builder.BuildAndStart();
        if (grpc_server_) {
            LOG_INFO("InterChatService已启动，监听: {}", server_address);
        } else {
            LOG_ERROR("InterChatService启动失败");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("启动InterChatService失败: {}", e.what());
    }
}