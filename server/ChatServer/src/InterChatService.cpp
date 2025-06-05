#include "InterChatService.h"
#include "Logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

InterChatServiceImpl::InterChatServiceImpl(ConnectionManager& conn_manager, MessageHandler& msg_handler)
    : conn_manager_(conn_manager), msg_handler_(msg_handler) {
    LOG_INFO("InterChatService 已初始化");
}

grpc::Status InterChatServiceImpl::ForwardMessage(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<ForwardedMessage, ForwardedMessage>* stream) {
    
    ForwardedMessage request;
    
    while (stream->Read(&request)) {
        LOG_INFO("收到转发消息: message_id={}, 从用户 {} 到用户 {}", 
                request.message_id(), request.original_sender_id(), request.target_recipient_id());
        
        try {
            // 将字符串转换为uint64_t
            uint64_t target_uid = std::stoull(request.target_recipient_id());
            
            // 在当前服务器上查找目标用户连接
            auto to_conn = conn_manager_.findConnectionByUid(target_uid);
            
            if (to_conn) {
                // 构造接收消息
                json message_json;
                message_json["type"] = MessageType::RecvMsg;
                
                json message_data;
                message_data["from_uid"] = std::stoull(request.original_sender_id());
                message_data["msg_type"] = "text"; // 默认为文本消息，可以在ForwardedMessage中添加消息类型字段
                message_data["content"] = request.content();
                message_data["timestamp"] = std::stoll(request.timestamp());
                
                message_json["data"] = message_data;
                
                // 创建要发送的消息
                Message forward_msg;
                forward_msg.msg_id = 0; // 系统消息，不需要回复
                forward_msg.content = message_json.dump();
                forward_msg.content_len = static_cast<uint16_t>(forward_msg.content.length());
                
                // 发送消息给目标用户
                to_conn->sendMessage(forward_msg);
                
                LOG_INFO("转发消息成功发送到用户 {}", target_uid);
                
                // 返回成功响应
                ForwardedMessage response;
                response.set_message_id(request.message_id());
                response.set_original_sender_id(request.original_sender_id());
                response.set_target_recipient_id(request.target_recipient_id());
                response.set_content("delivered");
                response.set_timestamp(request.timestamp());
                
                if (!stream->Write(response)) {
                    LOG_ERROR("写入响应失败");
                    return grpc::Status(grpc::StatusCode::INTERNAL, "写入响应失败");
                }
            } else {
                LOG_WARN("目标用户 {} 不在本服务器上", target_uid);
                
                // 返回失败响应
                ForwardedMessage response;
                response.set_message_id(request.message_id());
                response.set_original_sender_id(request.original_sender_id());
                response.set_target_recipient_id(request.target_recipient_id());
                response.set_content("user_not_found");
                response.set_timestamp(request.timestamp());
                
                if (!stream->Write(response)) {
                    LOG_ERROR("写入响应失败");
                    return grpc::Status(grpc::StatusCode::INTERNAL, "写入响应失败");
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("处理转发消息失败: {}", e.what());
            
            // 返回错误响应
            ForwardedMessage response;
            response.set_message_id(request.message_id());
            response.set_original_sender_id(request.original_sender_id());
            response.set_target_recipient_id(request.target_recipient_id());
            response.set_content("error: " + std::string(e.what()));
            response.set_timestamp(request.timestamp());
            
            if (!stream->Write(response)) {
                LOG_ERROR("写入错误响应失败");
            }
        }
    }
    
    return grpc::Status::OK;
} 