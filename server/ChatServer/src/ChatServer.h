#pragma once

#include "TcpConnection.h"
#include "ConnectionManager.h"
#include "MessageHandler.h"
#include "InterChatService.h"
#include <boost/asio.hpp>
#include <string>
#include "InterChat.grpc.pb.h"
#include <unordered_map>
#include <grpcpp/grpcpp.h>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class ChatServer : public std::enable_shared_from_this<ChatServer> {
public:
    ChatServer(asio::io_context &io_context, const uint16_t port);
    ~ChatServer();

    void start();
    void stop();

private:
    void do_accept();
    void handle_accept(TcpConnectionPtr conn, const boost::system::error_code& error);
    void handle_message(const TcpConnectionPtr& conn, const Message& msg) const;
    void handle_close(const TcpConnectionPtr& conn);
    void register_server();
    void send_heartbeat();
    void start_inter_chat_service();

private:
    asio::io_context &io_context_;
    tcp::acceptor acceptor_;
    std::string server_id_;
    std::string host_;
    std::string port_;
    int heartbeat_interval_;
    int connect_timeout_;
    
    asio::steady_timer heartbeat_timer_;
    
    ConnectionManager connection_manager_;
    std::unique_ptr<MessageHandler> message_handler_;
    
    // InterChatService相关
    std::unique_ptr<InterChatServiceImpl> inter_chat_service_;
    std::unique_ptr<grpc::Server> grpc_server_;
    int grpc_port_;
};