#include "NewChatServer.h"
#include "ConfigMgr.h"
#include "Logger.h"
#include "StatusGrpcClient.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

NewChatServer::NewChatServer() {
    auto& config = ConfigMgr::getInstance();

    host_ = config["ChatServer"].get("host", "0.0.0.0");
    grpc_port_ = config["ChatServer"].get("grpc_port", "50056");

    std::string redis_host = config["Redis"].get("host", "127.0.0.1");
    int redis_port = std::stoi(config["Redis"].get("port", "6379"));
    std::string redis_password = config["Redis"]["password"];

    presence_client_ = std::make_shared<PresenceClient>(redis_host, redis_port, redis_password);
    deliver_client_ = std::make_shared<DeliverApiClient>();
    chat_api_service_ = std::make_unique<ChatApiService>(presence_client_, deliver_client_);
}

NewChatServer::~NewChatServer() {
    Stop();
}

void NewChatServer::Start() {
    const std::string server_addr = host_ + ":" + grpc_port_;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(chat_api_service_.get());
    grpc_server_ = builder.BuildAndStart();

    LOG_INFO("NewChatServer listening on {}", server_addr);

    // Register with StatusServer (heartbeat kept for observability)
    try {
        auto& config = ConfigMgr::getInstance();
        std::string server_id = config["ChatServer"].get("server_id", "");
        if (server_id.empty()) {
            server_id = boost::uuids::to_string(boost::uuids::random_generator()());
        }
        StatusGrpcClient::getInstance().RegisterChatServer(
            server_id, host_, grpc_port_, "N/A", "N/A");
        LOG_INFO("NewChatServer registered with StatusServer, id={}", server_id);
    } catch (const std::exception& e) {
        LOG_WARN("NewChatServer: StatusServer registration failed: {}", e.what());
    }

    grpc_server_->Wait();
}

void NewChatServer::Stop() {
    if (grpc_server_) {
        grpc_server_->Shutdown();
        grpc_server_.reset();
        LOG_INFO("NewChatServer stopped");
    }
}
