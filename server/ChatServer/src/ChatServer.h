#pragma once

#include "TcpConnection.h"
#include "ConnectionManager.h"
#include "MessageHandler.h"
#include <boost/asio.hpp>
#include <string>
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
    void handle_accept(const TcpConnectionPtr &conn, const boost::system::error_code& error);
    void handle_message(const TcpConnectionPtr& conn, const Message& msg) const;
    void handle_close(const TcpConnectionPtr& conn);
    void register_server();
    void send_heartbeat();\
    void init_mq_gateway();
    void handle_mq_message(const std::string& routing_key, const std::string& message_type, 
                          uint64_t target_user_id, uint64_t sender_user_id, const std::string& payload);

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
    
    // MQ相关
    std::string message_queue_name_;
    std::string notification_queue_name_;
};