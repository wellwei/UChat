#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <queue>
#include <mutex>
#include <functional>
#include <string>
#include <vector>
#include <array>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// Forward declaration
class TcpConnection;

// Define shared pointer type for TcpConnection
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

// Message structure
struct Message {
    uint16_t msg_id;      // Message ID
    uint16_t content_len; // Length of message content
    std::string content;  // Message content (e.g., JSON string)
};

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    // Define constants
    static constexpr std::size_t kHeaderSize = 4;
    static constexpr std::size_t kMaxSendQueueSize = 1000; // Assuming a default value
    static constexpr uint16_t kMaxContentLen = 65535;    // Max value for uint16_t

    using MessageCallback = std::function<void(const TcpConnectionPtr&, const Message&)>;
    using CloseCallback = std::function<void(const TcpConnectionPtr&)>;

    TcpConnection(asio::io_context& io_context, uint32_t timeout_seconds);

    ~TcpConnection();

    void start();

    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = std::move(cb); }
    void setCloseCallback(const CloseCallback& cb) { closeCallback_ = std::move(cb); }

    void sendMessage(const Message& message);

    const std::string& id() const { return conn_id_; }
    uint64_t uid() const { return uid_; }
    bool isAuthenticated() const { return is_authenticated_; }
    tcp::socket& socket() { return socket_; }

    void setUid(uint64_t uid) { uid_ = uid; }
    void setAuthenticated(bool authenticated) { is_authenticated_ = authenticated; }

    void close();

private:
    void readHeader();

    void readBody(uint16_t msg_id, uint16_t content_len);

    void handleError(const boost::system::error_code& error);

    void checkTimeout();

    bool isExpired(const uint32_t timeout_seconds) const;

    void handleSendQueue(const boost::system::error_code& error, std::size_t /*length*/, const TcpConnectionPtr& self);

    void updateLastActiveTime();

    // Member variables
    tcp::socket socket_;
    std::string conn_id_;
    uint64_t uid_ = 0; // Initialize UID to 0
    bool is_authenticated_;

    // Read buffer
    std::array<uint8_t, kHeaderSize> header_; // Use constant
    std::vector<uint8_t> body_;

    // Send queue
    std::queue<std::vector<uint8_t>> send_queue_;
    std::mutex send_queue_mutex_;

    // Callbacks
    MessageCallback messageCallback_;
    CloseCallback closeCallback_;

    // Timeout related
    asio::steady_timer timeout_timer_;
    uint32_t timeout_seconds_;
    std::time_t last_active_time_;
};
