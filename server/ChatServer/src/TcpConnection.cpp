#include "TcpConnection.h"
#include "Logger.h"
#include "StatusGrpcClient.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

TcpConnection::TcpConnection(asio::io_context& io_context, const uint32_t timeout_seconds)
    : socket_(io_context),
      timeout_seconds_(timeout_seconds),
      timeout_timer_(io_context), 
      is_authenticated_(false) {
    // Generate unique connection ID
    conn_id_ = boost::uuids::to_string(boost::uuids::random_generator()());

    updateLastActiveTime();
    LOG_DEBUG("创建新的连接 ID: {}", conn_id_);
}

TcpConnection::~TcpConnection() {
    LOG_DEBUG("销毁连接 ID: {}", conn_id_);
}

void TcpConnection::start() {
    LOG_DEBUG("启动连接 ID: {}", conn_id_);
    readHeader();
    checkTimeout();
}

void TcpConnection::sendMessage(const Message& message) {
    std::lock_guard<std::mutex> lock(send_queue_mutex_);
    auto send_queue_size = send_queue_.size();
    if (send_queue_size >= kMaxSendQueueSize) {
        LOG_ERROR("发送队列已满，无法发送消息");
        return;
    }
    std::array<uint8_t, kHeaderSize> header;
    header[0] = (message.msg_id >> 8) & 0xFF;
    header[1] = message.msg_id & 0xFF;
    header[2] = (message.content_len >> 8) & 0xFF;
    header[3] = message.content_len & 0xFF;
    std::vector<uint8_t> buf(header.begin(), header.end());
    buf.insert(buf.end(), message.content.begin(), message.content.end());

    send_queue_.push(buf);
    if (send_queue_size > 0) {      // 前面有消息在发送，则不发送
        return;
    }
    // 队列中没有消息在发送，则发送消息
    auto &msg = send_queue_.front();
    asio::async_write(socket_, asio::buffer(msg.data(), msg.size()), std::bind(&TcpConnection::handleSendQueue, this, std::placeholders::_1, std::placeholders::_2, shared_from_this()));
}

// 发送消息队列, 保证有序
void TcpConnection::handleSendQueue(const boost::system::error_code& error, std::size_t /*length*/, const TcpConnectionPtr& self) {
    try {
        if (error) {
            LOG_ERROR("发送消息失败: {}", error.message());
            handleError(error);
            return;
        }
        std::lock_guard<std::mutex> lock(send_queue_mutex_);
        send_queue_.pop();
        if (!send_queue_.empty()) {
            auto &msg = send_queue_.front();
            asio::async_write(socket_, asio::buffer(msg.data(), msg.size()), std::bind(&TcpConnection::handleSendQueue, this, std::placeholders::_1, std::placeholders::_2, self));
        }
    } catch (const std::exception& e) {
        LOG_ERROR("发送消息失败: {}", e.what());
        handleError(error);
    }
}

void TcpConnection::close() {
    if (socket_.is_open()) {
        boost::system::error_code ec;
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }
}

void TcpConnection::readHeader() {
    auto self(shared_from_this());
    asio::async_read(socket_, asio::buffer(header_, kHeaderSize),
        [this, self](const boost::system::error_code& error, std::size_t /*length*/) {
            if (!error) {
                updateLastActiveTime();
                // Parse header: first 2 bytes for msg_len, next 2 bytes for content_len
                // 转为小端序
                uint16_t msg_id = (header_[0] << 8) | header_[1];
                uint16_t content_len = (header_[2] << 8) | header_[3];
                
                LOG_DEBUG("收到消息头: msg_id={}, 长度={}", msg_id, content_len);
                
                // Read message body
                readBody(msg_id, content_len);
            } else {
                handleError(error);
            }
        });
}

void TcpConnection::readBody(uint16_t msg_id, uint16_t content_len) {
    if (content_len > kMaxContentLen) {
        LOG_ERROR("消息体过长: {} > {}", content_len, kMaxContentLen);
        close();
        return;
    }
    
    auto self(shared_from_this());
    body_.resize(content_len);
    
    asio::async_read(socket_, asio::buffer(body_, content_len),
        [this, self, msg_id, content_len](const boost::system::error_code& error, std::size_t /*length*/) {
            if (!error) {
                updateLastActiveTime();
                
                // Process message
                try {
                    Message msg;
                    msg.msg_id = msg_id;
                    msg.content_len = content_len;
                    msg.content = std::string(body_.begin(), body_.end());
                    
                    // Call message callback if set
                    if (messageCallback_) {
                        messageCallback_(self, msg);
                    }
                    
                    // Continue reading next message
                    readHeader();
                } catch (const std::exception& e) {
                    LOG_ERROR("解析消息失败: {}", e.what());
                    readHeader(); // Try to recover by reading next message
                }
            } else {
                handleError(error);
            }
        });
}

void TcpConnection::handleError(const boost::system::error_code& error) {
    if (error != asio::error::operation_aborted) {
        LOG_ERROR("连接 ID: {} 发生错误: {}", conn_id_, error.message());
        
        // 如果是已认证的用户，更新用户在线状态
        if (is_authenticated_ && uid_ > 0) {
            try {
                auto reply = StatusGrpcClient::getInstance().UserOnlineStatusUpdate(uid_, false);
                if (reply.code() != UserOnlineStatusUpdateResponse_Code_OK) {
                    LOG_ERROR("更新用户离线状态失败: {}", reply.error_msg());
                } else {
                    LOG_INFO("用户 {} 离线状态已更新", uid_);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("更新用户离线状态异常: {}", e.what());
            }
        }
        
        if (closeCallback_) {
            closeCallback_(shared_from_this());
        }
        
        close();
    }
}

void TcpConnection::checkTimeout() {
    auto self(shared_from_this());
    timeout_timer_.expires_after(std::chrono::seconds(timeout_seconds_ / 2));
    timeout_timer_.async_wait([this, self](const boost::system::error_code& error) {
        if (!error) {
            if (isExpired(timeout_seconds_)) {
                LOG_INFO("连接 ID: {} 超时", conn_id_);
                if (closeCallback_) {
                    closeCallback_(self);
                }
                close();
            } else {
                checkTimeout(); // Schedule next check
            }
        }
    });
}

bool TcpConnection::isExpired(const uint32_t timeout_seconds) const {
    return (time(nullptr) - last_active_time_) > timeout_seconds;
}

void TcpConnection::updateLastActiveTime() {
    last_active_time_ = time(nullptr);
}
