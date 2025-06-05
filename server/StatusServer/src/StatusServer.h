#pragma once

#include <grpcpp/grpcpp.h>  
#include "proto_generated/StatusServer.grpc.pb.h"
#include <chrono>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <memory>

struct ChatServerInfo {
    std::string server_id;
    std::string host;
    std::string port;
    uint64_t current_connections;
    std::chrono::steady_clock::time_point last_heartbeat;
};

class StatusServer : public StatusService::Service {
private:
    std::unordered_map<std::string, ChatServerInfo> chat_servers_;
    std::mutex mutex_;

    // uid 所在 ChatServer
    std::unordered_map<uint64_t, std::string> uid_chat_server_;

    // ChatServer 要在 kHeartbeatTimeout_ 时间内发送心跳，否则会被删除视为失效
    std::chrono::seconds kHeartbeatTimeout_{};
    
public:
    StatusServer() {
        // 启动定时清理失效的 ChatServer
        std::thread(&StatusServer::pruneDeadServers, this).detach();
    }

    void start();

    // grpc 服务方法实现

    grpc::Status GetChatServer(grpc::ServerContext* context, const GetChatServerRequest* request, GetChatServerResponse* response) override;

    grpc::Status RegisterChatServer(grpc::ServerContext* context, const RegisterChatServerRequest* request, RegisterChatServerResponse* response) override;

    grpc::Status ChatServerHeartbeat(grpc::ServerContext* context, const ChatServerHeartbeatRequest* request, ChatServerHeartbeatResponse* response) override;

    grpc::Status UnregisterChatServer(grpc::ServerContext* context, const UnregisterChatServerRequest* request, UnregisterChatServerResponse* response) override;

    grpc::Status UserOnlineStatusUpdate(grpc::ServerContext* context, const UserOnlineStatusUpdateRequest* request, UserOnlineStatusUpdateResponse* response) override;

    grpc::Status GetUidChatServer(grpc::ServerContext* context, const GetUidChatServerRequest* request, GetUidChatServerResponse* response) override;

private:
    // 定期清理失效的 ChatServer
    void pruneDeadServers();

    std::unique_ptr<grpc::Server> server_;
};