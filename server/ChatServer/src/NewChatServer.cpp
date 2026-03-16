#include "NewChatServer.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "Logger.h"

NewChatServer::NewChatServer() {
    auto& config = ConfigMgr::getInstance();

    host_ = config["ChatServer"].get("host", "0.0.0.0");
    grpc_port_ = config["ChatServer"].get("grpc_port", "50056");

    // Initialize RedisMgr singleton
    RedisMgr::getInstance();

    deliver_client_ = std::make_shared<DeliverApiClient>();
    chat_api_service_ = std::make_unique<ChatApiService>(deliver_client_);
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

    grpc_server_->Wait();
}

void NewChatServer::Stop() {
    if (grpc_server_) {
        grpc_server_->Shutdown();
        grpc_server_.reset();
        LOG_INFO("NewChatServer stopped");
    }
}
