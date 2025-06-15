#pragma once

#include "TcpConnection.h"
#include "ConnectionManager.h"
#include "MQGatewayClient.h"
#include <jwt-cpp/jwt.h>
#include <string>
#include <nlohmann/json.hpp>
#include <atomic>

using json = nlohmann::json;

class MessageHandler {
public:
    struct MessageTask {
        TcpConnectionPtr conn;
        Message msg;
    };

    MessageHandler(ConnectionManager& conn_manager, const std::string& jwt_secret_key, const std::string& server_id);
    ~MessageHandler();

    void handleMessage(const TcpConnectionPtr& conn, const Message& msg);

private:
    // 启动处理消息线程
    void workerThread();

    void processMessage(const MessageTask &task);

    // 处理认证消息
    bool handleAuthMessage(const TcpConnectionPtr& conn, const uint16_t req_id, const json& j);

    // 处理聊天消息
    void handleChatMessage(const TcpConnectionPtr& conn, const uint16_t req_id, const json& j);

    // 处理心跳消息
    void handleHeartbeatMessage(const TcpConnectionPtr& conn, const uint16_t req_id);

    // 广播消息
    void broadcastMessage(const Message& msg, uint64_t exclude_uid = 0);

    // 发送JSON响应
    void sendJsonResponse(const TcpConnectionPtr& conn, const uint16_t req_id, const std::string& type, const int32_t error_code, const std::string& message);

private:
    ConnectionManager& conn_manager_;
    jwt::verifier<jwt::default_clock, jwt::traits::kazuho_picojson> verifier_;
    std::string jwt_secret_key_;
    std::string server_id_;

    std::thread worker_thread_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<MessageTask> message_queue_;
    std::atomic_bool stop_flag_;
}; 

namespace MessageType {
    static const std::string AuthReq = "auth_req";              // 认证请求
    static const std::string AuthResp = "auth_resp";            // 认证响应
    static const std::string HeartbeatReq = "heartbeat_req";    // 心跳请求
    static const std::string HeartbeatResp = "heartbeat_resp";  // 心跳响应
    static const std::string SendMsgReq = "send_msg_req";       // 客户端发送消息请求
    static const std::string SendMsgResp = "send_msg_resp";     // 客户端发送消息响应
    static const std::string RecvMsg = "recv_msg";              // 接收到并转发给目标客户端的消息
    static const std::string ErrorResp = "error_resp";          // 错误响应
};

