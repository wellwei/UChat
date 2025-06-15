#include "MessageHandler.h"
#include <nlohmann/json.hpp>
#include <ctime>
#include "Logger.h"
#include "error_codes.h"
#include "StatusGrpcClient.h"
#include "ConfigMgr.h"
using json = nlohmann::json;

MessageHandler::MessageHandler(ConnectionManager &conn_manager, const std::string &jwt_secret_key, const std::string &server_id)
    : conn_manager_(conn_manager),
      server_id_(server_id),
      verifier_(jwt::verify()
          .allow_algorithm(jwt::algorithm::hs256{jwt_secret_key})
          .with_issuer("UserService")),
      jwt_secret_key_(jwt_secret_key) {
    // 启动处理消息线程
    worker_thread_ = std::thread([this]() { workerThread(); });
}

MessageHandler::~MessageHandler() {
    stop_flag_.store(true);
    queue_cv_.notify_all();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}


void MessageHandler::handleMessage(const TcpConnectionPtr &conn, const Message &msg) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        message_queue_.push({conn, msg});
        queue_cv_.notify_one();
    }
}

void MessageHandler::workerThread() {
    LOG_INFO("消息处理线程启动");

    while (!stop_flag_) {
        MessageTask task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return !message_queue_.empty() || stop_flag_;});

            if (stop_flag_ && message_queue_.empty()) {
                break;
            }

            task = std::move(message_queue_.front());
            message_queue_.pop();
        }

        processMessage(task);
    }
}

void MessageHandler::processMessage(const MessageTask &task) {
    const auto &conn = task.conn;
    const auto &msg = task.msg;
    const uint16_t req_id = msg.msg_id;

    try {
        auto j = json::parse(msg.content);
        
        if (!j.contains("type") || !j["type"].is_string()) {
            LOG_WARN("连接ID: {}: 消息缺少或无效的 'type' 字段 {}", conn->id(), msg.msg_id);
            sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrCommonInvalidField, "无效消息格式：缺少或无效的 'type' 字段");
            return;
        }

        if (!j.contains("data") || !j["data"].is_object()) {
            LOG_WARN("连接ID: {}: 消息缺少或无效的 'data' 字段 {}", conn->id(), msg.msg_id);
            sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrCommonInvalidField, "无效消息格式：缺少或无效的 'data' 字段");
            return;
        }

        const auto data = j["data"];

        const std::string type = j["type"];

        if (type == MessageType::AuthReq) {
            handleAuthMessage(conn, req_id, data);
        } else if (type == MessageType::SendMsgReq) {
            handleChatMessage(conn, req_id, data);
        } else if (type == MessageType::HeartbeatReq) {
            handleHeartbeatMessage(conn, req_id);
        } else {
            sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrCommonInvalidType, "无效消息类型");
        }
    } catch (const json::parse_error &e) {
        LOG_ERROR("JSON解析错误: {}", e.what());
        sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrCommonInvalidJson, "无效JSON格式");
    } catch (const std::exception &e) {
        LOG_ERROR("处理消息失败: {}", e.what());
        sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrCommonInvalidJson, "无效消息格式");
    }
}

bool MessageHandler::handleAuthMessage(const TcpConnectionPtr &conn, const uint16_t req_id, const nlohmann::json &data) {
    if (!data.contains("token")) {
        sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrChatTokenMissing, "Authentication token is required");
        return false;
    }

    try {
        const std::string token = data["token"];
        const std::string suid = data.contains("uid") ? data["uid"].get<std::string>() : "";
        const uint64_t uid = std::stoull(suid);

        auto decoded = jwt::decode(token);
        verifier_.verify(decoded);

        if (decoded.get_subject() != suid) {
            LOG_WARN("用户 ID 不匹配: 令牌中的用户 ID 为 {}, 请求中的用户 ID 为 {}", decoded.get_subject(), uid);
            sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrChatTokenUserMismatch, "用户 ID 不匹配");
            return false;
        }

        conn->setUid(uid);
        conn->setAuthenticated(true);

        // 将用户连接添加到连接管理器
        conn_manager_.addUserConnection(uid, conn);

        // 更新用户上线状态，注册用户到当前服务器
        auto &config = ConfigMgr::getInstance();
        std::string server_id = config["ChatServer"].get("server_id", "");
        
        auto reply = StatusGrpcClient::getInstance().UserOnlineStatusUpdate(uid, server_id, true);
        if (reply.code() != UserOnlineStatusUpdateResponse_Code_OK) {
            LOG_ERROR("更新用户 {} 上线状态失败: {}", uid, reply.error_msg());
        } else {
            LOG_INFO("用户 {} 已注册到服务器 {}", uid, server_id);
        }

        // 发送认证成功消息
        sendJsonResponse(conn, req_id, MessageType::AuthResp, ErrorCodes::kSuccess, "认证成功");

         LOG_INFO("用户 {} 认证成功", uid);
        return true;
    } catch (const jwt::error::token_verification_exception &e) {
        LOG_WARN("JWT 认证失败: {}", e.what());
        sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrChatTokenInvalid, "Invalid authentication token");
        return false;
    } catch (const jwt::error::signature_verification_exception &e) {
        LOG_WARN("JWT 签名认证失败: {}", e.what());
        sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrChatTokenInvalid, "Invalid authentication token");
        return false;
    } catch (const std::exception &e) {
        LOG_ERROR("JWT 认证失败: {}", e.what());
        sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrChatTokenInvalid, "Invalid authentication token");
        return false;
    }
}

void MessageHandler::handleChatMessage(const TcpConnectionPtr &conn, const uint16_t req_id, const nlohmann::json &data) {
    if (!conn->isAuthenticated()) {
        sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrChatUnauthenticated, "Unauthenticated user");
        return;
    }

    if (!data.contains("msg_type") || !data["msg_type"].is_string()) {
        sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrCommonInvalidField, "无效消息类型");
        return;
    }

    if (!data.contains("content") || !data["content"].is_string()) {
        sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrCommonInvalidField, "无效消息内容");
        return;
    }
    LOG_INFO("to_uid: {}", data["to_uid"].get<std::string>());
    if (!data.contains("to_uid") || !data["to_uid"].is_string()) {
        sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrCommonInvalidField, "无效消息目标用户ID");
        return;
    }

    const std::string msg_type = data["msg_type"];
    const std::string content = data["content"];
    const uint64_t to_uid = std::stoull(data["to_uid"].get<std::string>());
    const uint64_t from_uid = conn->uid();

    if (to_uid == from_uid) {
        sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrCommonInvalidField, "不能发送消息给自己");
        return;
    }

    // 检查目标用户是否在线（先查询本地连接）
    TcpConnectionPtr to_conn = conn_manager_.findConnectionByUid(to_uid);
    
    // 如果目标用户不在本服务器，查询状态服务获取目标用户所在的Chat服务器
    if (!to_conn) {
        try {
            auto reply = StatusGrpcClient::getInstance().GetUidChatServer(to_uid);
            
            if (reply.code() == GetUidChatServerResponse_Code_OK) {
                // 用户在其他服务器上在线，通过MQ转发消息
                LOG_INFO("用户 {} 在其他聊天服务器上在线，队列: {}", 
                         to_uid, reply.message_queue_name());
                
                // 构造消息payload
                json message_payload;
                message_payload["from_uid"] = from_uid;
                message_payload["to_uid"] = to_uid;
                message_payload["content"] = content;
                message_payload["msg_type"] = msg_type;
                message_payload["timestamp"] = std::time(nullptr);
                
                // 使用MQGateway转发消息
                bool forwarded = MQGatewayClient::getInstance().publishMessage(
                    reply.message_queue_name(),  // 目标ChatServer的消息队列
                    "chat_message",
                    to_uid,
                    from_uid,
                    message_payload.dump()
                );
                
                if (forwarded) {
                    // 告知发送者消息已接收，正在转发
                    sendJsonResponse(conn, req_id, MessageType::SendMsgResp, ErrorCodes::kSuccess, "消息已接收，正在转发");
                } else {
                    // 转发失败
                    LOG_ERROR("通过MQ转发消息失败，目标队列: {}", reply.message_queue_name());
                    sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrChatSendFailed, "消息转发失败");
                }
                return;
            } else if (reply.code() == GetUidChatServerResponse_Code_USER_OFFLINE) {
                // 用户离线，可以存储离线消息
                LOG_INFO("用户 {} 当前离线，存储离线消息", to_uid);
                
                // TODO: 实现离线消息存储
                // 通过 UserServer 搭配 MQ 实现
                
                sendJsonResponse(conn, req_id, MessageType::SendMsgResp, ErrorCodes::kSuccess, "用户离线，消息已存储");
                return;
            } else {
                LOG_ERROR("获取用户 {} 所在服务器失败: {}", to_uid, reply.error_msg());
                sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrChatUserNotFound, "用户不在线或不存在");
                return;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("查询用户 {} 状态异常: {}", to_uid, e.what());
            sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrChatServiceUnavailable, "服务暂时不可用");
            return;
        }
    }
    
    // 目标用户在本服务器上在线，直接发送消息
    try {
        // 构建发送给目标用户的消息
        json message_json;
        message_json["type"] = MessageType::RecvMsg;
        
        json message_data;
        message_data["from_uid"] = std::to_string(from_uid);
        message_data["msg_type"] = msg_type;
        message_data["content"] = content;
        message_data["timestamp"] = static_cast<uint64_t>(std::time(nullptr));
        
        message_json["data"] = message_data;
        
        // 创建要发送的消息
        Message forward_msg;
        forward_msg.msg_id = 0; // 系统消息，不需要回复
        forward_msg.content = message_json.dump();
        forward_msg.content_len = static_cast<uint16_t>(forward_msg.content.length());
        
        // 发送消息给目标用户
        to_conn->sendMessage(forward_msg);
        
        LOG_INFO("消息已从用户 {} 发送到用户 {}", from_uid, to_uid);
        
        // 更新用户状态
        try {
            auto reply = StatusGrpcClient::getInstance().UserOnlineStatusUpdate(to_uid, server_id_, true);
            if (reply.code() != UserOnlineStatusUpdateResponse_Code_OK) {
                LOG_WARN("更新用户 {} 状态失败: {}", to_uid, reply.error_msg());
            }
        } catch (const std::exception& e) {
            LOG_ERROR("更新用户状态异常: {}", e.what());
        }
        
        // 告知发送者消息已发送成功
        sendJsonResponse(conn, req_id, MessageType::SendMsgResp, ErrorCodes::kSuccess, "消息发送成功");
    } catch (const std::exception& e) {
        LOG_ERROR("发送消息给用户 {} 失败: {}", to_uid, e.what());
        sendJsonResponse(conn, req_id, MessageType::ErrorResp, ErrorCodes::kErrChatSendFailed, "发送消息失败");
    }
}

void MessageHandler::handleHeartbeatMessage(const TcpConnectionPtr &conn, const uint16_t req_id) {
    sendJsonResponse(conn, req_id, MessageType::HeartbeatResp, ErrorCodes::kSuccess, "Pong");
}

void MessageHandler::broadcastMessage(const Message &msg, uint64_t exclude_uid) {
    auto &user_connections = conn_manager_.getAllUserConnections();

    for (const auto &[uid, conn]: user_connections) {
        if (uid != exclude_uid) {
            conn->sendMessage(msg);
        }
    }
}

void MessageHandler::sendJsonResponse(const TcpConnectionPtr &conn, const uint16_t req_id, const std::string &type, const int32_t error_code, const std::string &message) {
    try {
        json res_json;
        res_json["type"] = type;
        res_json["code"] = error_code;
        res_json["message"] = message;

        Message res_msg;
        res_msg.msg_id = req_id;
        res_msg.content = res_json.dump();
        res_msg.content_len = static_cast<uint16_t>(res_msg.content.length());

        conn->sendMessage(res_msg);
    } catch (const std::exception& e) {
        LOG_ERROR("发送JSON响应失败: {}", e.what());
    }
}
