#include "GatewayServer.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"
#include "Logger.h"

GatewayServer::GatewayServer() {
    auto config = *ConfigMgr::getInstance();
    gateway_id_ = config["GatewayServer"]["gateway_id"];
    grpc_host_ = config["GatewayServer"]["grpc_host"];
    grpc_port_ = std::stoi(config["GatewayServer"].get("grpc_port", "50060"));
    online_ttl_seconds_ = std::stoi(config["PresenceTTL"].get("online_ttl_seconds", "60"));
    gateway_ttl_ = std::stoi(config["GatewayServer"].get("gateway_ttl", "300"));

    tunnel_service_ = std::make_unique<ChatTunnelService>(&registry_, gateway_id_, online_ttl_seconds_);
    deliver_service_ = std::make_unique<DeliverApiService>(&registry_);
}

GatewayServer::~GatewayServer() {
    Stop();
}

void GatewayServer::Start() {
    // Write gateway presence: gateway:{gateway_id} -> "host:grpc_port"
    // Use public_host if grpc_host is 0.0.0.0
    std::string publish_host = grpc_host_;
    if (publish_host == "0.0.0.0") {
        publish_host = ConfigMgr::getInstance()->operator[]("GatewayServer").get("public_host", "localhost");
    }
    deliver_addr_ = publish_host + ":" + std::to_string(grpc_port_);
    RedisMgr::getInstance()->set("gateway:" + gateway_id_, deliver_addr_, gateway_ttl_);
    LOG_INFO("GatewayServer: registered gateway_id={}, deliver_addr={}", gateway_id_, deliver_addr_);

    std::string listen_addr = grpc_host_ + ":" + std::to_string(grpc_port_);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(listen_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(tunnel_service_.get());
    builder.RegisterService(deliver_service_.get());
    grpc_server_ = builder.BuildAndStart();

    if (!grpc_server_) {
        LOG_ERROR("GatewayServer: Failed to start gRPC server on {}", listen_addr);
        return;
    }

    stopped_ = false;
    LOG_INFO("GatewayServer listening on {}", listen_addr);

    // Launch heartbeat thread: refresh gateway presence key every gateway_ttl/2 seconds
    running_ = true;
    heartbeat_thread_ = std::thread([this] {
        while (running_) {
            std::unique_lock<std::mutex> lock(heartbeat_mu_);
            heartbeat_cv_.wait_for(lock,
                std::chrono::seconds(gateway_ttl_ / 2),
                [this] { return !running_.load(); });
            if (!running_) break;
            // Use set (not expire) to auto-recover if key was accidentally evicted
            RedisMgr::getInstance()->set("gateway:" + gateway_id_, deliver_addr_, gateway_ttl_);
            LOG_DEBUG("GatewayServer: refreshed gateway presence for id={}", gateway_id_);
        }
    });
}

void GatewayServer::Wait() const {
    if (grpc_server_) {
        grpc_server_->Wait();
    }
}

void GatewayServer::Stop() {
    // 1. Stop heartbeat thread
    {
        std::lock_guard<std::mutex> lock(heartbeat_mu_);
        running_ = false;
    }
    heartbeat_cv_.notify_one();
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }

    // 2. Shut down gRPC server
    if (grpc_server_) {
        grpc_server_->Shutdown();
        grpc_server_.reset();
    }

    // 3. Remove gateway key from Redis (always, regardless of grpc_server_ state)
    if (!stopped_) {
        stopped_ = true;
        RedisMgr::getInstance()->del("gateway:" + gateway_id_);
        LOG_INFO("GatewayServer stopped");
    }
}
