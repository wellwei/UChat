#include "GatewayServer.h"
#include "ChatSession.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"
#include "Logger.h"

GatewayServer::GatewayServer() {
    auto config = *ConfigMgr::getInstance();
    gateway_id_ = config["GatewayServer"]["gateway_id"];
    grpc_host_ = config["GatewayServer"]["grpc_host"];
    grpc_port_ = std::stoi(config["GatewayServer"].get("grpc_port", "50060"));
    gateway_ttl_ = std::stoi(config["GatewayServer"].get("gateway_ttl", "60"));

    tunnel_service_ = std::make_unique<ChatTunnelService>(&registry_, gateway_id_);
    deliver_service_ = std::make_unique<DeliverApiService>(&registry_);
    control_service_ = std::make_unique<ChatControlApiService>();
}

GatewayServer::~GatewayServer() {
    Stop();
}

void GatewayServer::Start() {

    deliver_addr_ = publish_host_ + ":" + std::to_string(grpc_port_);
    RedisMgr::getInstance()->set("gateway:" + gateway_id_, deliver_addr_, gateway_ttl_);
    LOG_INFO("GatewayServer: registered gateway_id={}, deliver_addr={}", gateway_id_, deliver_addr_);

    this->StartPresenceRefreshTask();
    tunnel_service_->StartPresenceRefreshTask();

    std::string listen_addr = grpc_host_ + ":" + std::to_string(grpc_port_);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(listen_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(tunnel_service_.get());
    builder.RegisterService(deliver_service_.get());
    builder.RegisterService(control_service_.get());
    grpc_server_ = builder.BuildAndStart();

    if (!grpc_server_) {
        LOG_ERROR("GatewayServer: Failed to start gRPC server on {}", listen_addr);
        return;
    }

    stopped_ = false;
    LOG_INFO("GatewayServer listening on {}", listen_addr);


}

void GatewayServer::Wait() const {
    if (grpc_server_) {
        grpc_server_->Wait();
    }
}

void GatewayServer::Stop() {

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

void GatewayServer::RefreshPresenceRoutine() {
    deliver_addr_ = publish_host_ + ":" + std::to_string(grpc_port_);
    RedisMgr::getInstance()->set("gateway:" + gateway_id_, deliver_addr_, gateway_ttl_);

    TimerWheel::getInstance()->AddTask(20000, [this] {
        RefreshPresenceRoutine();
    });
}

void GatewayServer::StartPresenceRefreshTask() {
    TimerWheel::getInstance()->AddTask(20000, [this]() { RefreshPresenceRoutine(); });
}