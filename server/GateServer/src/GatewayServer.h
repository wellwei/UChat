#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "SessionRegistry.h"
#include "ChatTunnelService.h"
#include "DeliverApiService.h"

// Owns and manages the gRPC server that serves both ChatTunnel and DeliverApi.
// Start() registers the gateway presence in Redis and launches a heartbeat thread
// that periodically refreshes the TTL (every gateway_ttl/2 seconds).
// Stop() removes the gateway key and shuts down.
class GatewayServer {
public:
    GatewayServer();
    ~GatewayServer();

    void Start();
    void Wait() const;   // 阻塞直到 Stop() 被调用
    void Stop();

private:
    std::string gateway_id_;
    std::string grpc_host_;
    int grpc_port_ = 50060;
    int online_ttl_seconds_ = 60;
    int gateway_ttl_ = 300;          // gateway:{id} 的 TTL（秒）
    bool stopped_ = false;

    std::string deliver_addr_;       // 缓存发布地址，供刷新线程复用

    std::atomic<bool> running_{false};
    std::thread heartbeat_thread_;
    std::mutex heartbeat_mu_;
    std::condition_variable heartbeat_cv_;

    SessionRegistry registry_;
    std::unique_ptr<ChatTunnelService> tunnel_service_;
    std::unique_ptr<DeliverApiService> deliver_service_;

    std::unique_ptr<grpc::Server> grpc_server_;
};
