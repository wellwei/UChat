#pragma once

#include "GrpcStubPool.h"
#include "Singleton.h"
#include "statusserver.grpc.pb.h"

using grpc::Status;
using grpc::ClientContext;
using grpc::Channel;

class StatusGrpcClient : public Singleton<StatusGrpcClient> {
    friend class Singleton<StatusGrpcClient>;

public:
    GetChatServerResponse GetChatServer(const uint64_t &uid);
    RegisterChatServerResponse RegisterChatServer(const std::string &server_id, const std::string &host, const std::string &port, 
                                                 const std::string &message_queue_name, const std::string &notification_queue_name);
    UnregisterChatServerResponse UnregisterChatServer(const std::string &server_id);
    ChatServerHeartbeatResponse ChatServerHeartbeat(const std::string &server_id, const uint64_t &current_connections);
    UserOnlineStatusUpdateResponse UserOnlineStatusUpdate(const uint64_t &uid, const std::string &server_id, const bool &online);
    GetUidChatServerResponse GetUidChatServer(const uint64_t &uid);
private:
    StatusGrpcClient();
    std::unique_ptr<GrpcStubPool<StatusService>> stubs_;
};