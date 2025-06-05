#pragma once

#include "TcpConnection.h"
#include "ConnectionManager.h"
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

    MessageHandler(ConnectionManager& conn_manager, const std::string& jwt_secret_key);
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

    std::thread worker_thread_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<MessageTask> message_queue_;
    std::atomic_bool stop_flag_;
}; 

namespace MessageType {
    static const std::string AuthReq = "auth_req";
    static const std::string AuthResp = "auth_resp";
    static const std::string HeartbeatReq = "heartbeat_req";
    static const std::string HeartbeatResp = "heartbeat_resp";
    static const std::string SendMsgReq = "send_msg_req";
    static const std::string SendMsgResp = "send_msg_resp";
    static const std::string RecvMsg = "recv_msg";
    static const std::string ErrorResp = "error_resp";
};

