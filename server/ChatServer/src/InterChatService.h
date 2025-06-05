#pragma once

#include "proto_generated/InterChat.grpc.pb.h"
#include "ConnectionManager.h"
#include "MessageHandler.h"
#include <grpcpp/grpcpp.h>
#include <memory>

class InterChatServiceImpl final : public InterChatService::Service {
public:
    InterChatServiceImpl(ConnectionManager& conn_manager, MessageHandler& msg_handler);
    
    grpc::Status ForwardMessage(
        grpc::ServerContext* context,
        grpc::ServerReaderWriter<ForwardedMessage, ForwardedMessage>* stream) override;
        
private:
    ConnectionManager& conn_manager_;
    MessageHandler& msg_handler_;
}; 