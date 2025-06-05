#include "StatusServer.h"
#include "ConfigMgr.h"
#include "Logger.h"

void StatusServer::start() {
    auto &config = *ConfigMgr::getInstance();

    kHeartbeatTimeout_ = std::chrono::seconds(std::stoul(config["StatusServer"].get("heartbeat_timeout", "30")));
    std::string host = config["StatusServer"].get("host", "localhost");
    std::string port = config["StatusServer"].get("port", "50052");
    std::string server_address = host + ":" + port;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(this);

    server_ = builder.BuildAndStart();
    LOG_INFO("StatusServer 启动成功，监听地址: {}", server_address);

    server_->Wait();
}

void StatusServer::pruneDeadServers() {
    while (true) {
        // 每隔 kHeartbeatTimeout_ 时间检查一次
        std::this_thread::sleep_for(kHeartbeatTimeout_);

        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();

        for (auto it = chat_servers_.begin(); it != chat_servers_.end();) {
            if ((now - it->second.last_heartbeat) > kHeartbeatTimeout_) {
                LOG_WARN("ChatServer {} 心跳超时，已从状态服务器注销", it->first);
                it = chat_servers_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

grpc::Status StatusServer::GetChatServer(grpc::ServerContext* context, const GetChatServerRequest* request, GetChatServerResponse* response) {
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_DEBUG("GetChatServer 收到 {} 请求", request->uid());

    std::string host;
    std::string port;
    uint64_t min_connections = INT64_MAX;

    // 选择连接数最少的 ChatServer
    for (const auto& pair : chat_servers_) {
        if (pair.second.current_connections < min_connections) {
            auto time_diff = std::chrono::steady_clock::now() - pair.second.last_heartbeat;
            if (time_diff <= kHeartbeatTimeout_) {
                min_connections = pair.second.current_connections;
                host = pair.second.host;
                port = pair.second.port;
            }
        }
    }
    
    if (host.empty()) {
        LOG_WARN("没有可用的 ChatServer");
        response->set_code(GetChatServerResponse::Code::GetChatServerResponse_Code_NO_SERVER_AVAILABLE);
        response->set_error_msg("没有可用的 ChatServer");
        return grpc::Status::OK;
    }
    
    response->set_code(GetChatServerResponse::Code::GetChatServerResponse_Code_OK);
    response->set_host(host);
    response->set_port(port);

    LOG_DEBUG("GetChatServer 返回 ChatServer: {} {}", host, port);
    return grpc::Status::OK;
}

grpc::Status StatusServer::RegisterChatServer(grpc::ServerContext* context, const RegisterChatServerRequest* request, RegisterChatServerResponse* response) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto server_id = request->server_id();
    auto host = request->host();
    auto port = request->port();

    if (server_id.empty() || host.empty() || port.empty()) {
        LOG_WARN("RegisterChatServer 收到无效请求: server_id: {}, host: {}, port: {}", server_id, host, port);
        response->set_code(RegisterChatServerResponse::Code::RegisterChatServerResponse_Code_INVALID_REQUEST);
        response->set_error_msg("无效的请求");
        return grpc::Status::OK;
    }   

    if (chat_servers_.find(server_id) != chat_servers_.end()) {
        LOG_WARN("ChatServer {} 已存在, 将更新其信息", server_id);
        chat_servers_.erase(server_id);
        chat_servers_[server_id] = {server_id, host, port, 0, std::chrono::steady_clock::now()};
        response->set_code(RegisterChatServerResponse::Code::RegisterChatServerResponse_Code_ALREADY_REGISTERED);
        return grpc::Status::OK;
    }

    chat_servers_[server_id] = {server_id, host, port, 0, std::chrono::steady_clock::now()};

    LOG_INFO("来自 {} 的 ChatServer {} 注册成功", host + ":" + port, server_id);
    response->set_code(RegisterChatServerResponse::Code::RegisterChatServerResponse_Code_OK);
    return grpc::Status::OK;
}

grpc::Status StatusServer::ChatServerHeartbeat(grpc::ServerContext* context, const ChatServerHeartbeatRequest* request, ChatServerHeartbeatResponse* response) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string& server_id = request->server_id();

    auto it = chat_servers_.find(server_id);
    if (it == chat_servers_.end()) {
        LOG_WARN("收到未知 ChatServer {} 的心跳", server_id);
        response->set_code(ChatServerHeartbeatResponse::Code::ChatServerHeartbeatResponse_Code_UNKNOWN_SERVER);
        response->set_error_msg("服务器未注册");
        return grpc::Status::OK;
    }

    it->second.current_connections = request->current_connections();
    it->second.last_heartbeat = std::chrono::steady_clock::now();

    LOG_DEBUG("ChatServer {} 更新心跳信息", server_id);
    response->set_code(ChatServerHeartbeatResponse::Code::ChatServerHeartbeatResponse_Code_OK);
    return grpc::Status::OK;
}

grpc::Status StatusServer::UnregisterChatServer(grpc::ServerContext* context, const UnregisterChatServerRequest* request, UnregisterChatServerResponse* response) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string& server_id = request->server_id();

    auto it = chat_servers_.find(server_id);
    if (it == chat_servers_.end()) {
        LOG_WARN("收到未知 ChatServer {} 的注销请求", server_id);
        response->set_code(UnregisterChatServerResponse::Code::UnregisterChatServerResponse_Code_UNKNOWN_SERVER);
        response->set_error_msg("服务器未注册");
        return grpc::Status::OK;
    }

    chat_servers_.erase(it);
    LOG_DEBUG("ChatServer {} 注销成功", server_id);
    response->set_code(UnregisterChatServerResponse::Code::UnregisterChatServerResponse_Code_OK);
    return grpc::Status::OK;
}

grpc::Status StatusServer::UserOnlineStatusUpdate(grpc::ServerContext* context, const UserOnlineStatusUpdateRequest* request, UserOnlineStatusUpdateResponse* response) {
    std::lock_guard<std::mutex> lock(mutex_);
    const uint64_t uid = request->uid();
    const bool online = request->online();
    const std::string& server_id = request->server_id();

    if (online) {
        uid_chat_server_[uid] = server_id;
    } else {
        uid_chat_server_.erase(uid);
    }

    response->set_code(UserOnlineStatusUpdateResponse::Code::UserOnlineStatusUpdateResponse_Code_OK);   
    return grpc::Status::OK;    
}

grpc::Status StatusServer::GetUidChatServer(grpc::ServerContext* context, const GetUidChatServerRequest* request, GetUidChatServerResponse* response) {
    std::lock_guard<std::mutex> lock(mutex_);
    const uint64_t uid = request->uid();
    const std::string& server_id = uid_chat_server_[uid];

    if (server_id.empty()) {
        response->set_code(GetUidChatServerResponse::Code::GetUidChatServerResponse_Code_USER_OFFLINE);
        response->set_error_msg("用户离线");
        return grpc::Status::OK;
    }

    auto it = chat_servers_.find(server_id);
    if (it == chat_servers_.end()) {
        response->set_code(GetUidChatServerResponse::Code::GetUidChatServerResponse_Code_INTERNAL_ERROR);
        response->set_error_msg("ChatServer 不存在");
        return grpc::Status::OK;
    }
    
    response->set_code(GetUidChatServerResponse::Code::GetUidChatServerResponse_Code_OK);
    response->set_host(it->second.host);
    response->set_port(it->second.port);
    return grpc::Status::OK;
}
