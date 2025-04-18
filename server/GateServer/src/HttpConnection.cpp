//
// Created by wellwei on 2025/4/1.
//


#include "HttpConnection.h"
#include "LogicSystem.h"
#include "util.h"
#include "Logger.h"


HttpConnection::HttpConnection(tcp::socket socket)
        : socket_(std::move(socket)) {
}

HttpConnection::HttpConnection(asio::io_context &ioc)
        : socket_(ioc) {
}

void HttpConnection::start() {
    doRead();
}

tcp::socket &HttpConnection::getSocket() {
    return socket_;
}

// 接收请求
void HttpConnection::doRead() {
    req_ = {}; // 清空请求体
    res_ = {}; // 清空响应体
    resetDeadline();

    auto self = shared_from_this();
    http::async_read(socket_, buffer_, req_, [self](beast::error_code ec, std::size_t bytes_transferred) {
        self->onRead(ec, bytes_transferred);
    });
}

// 接收请求完成的回调函数
void HttpConnection::onRead(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec == asio::error::operation_aborted) {
        LOG_WARN("Read operation aborted");
        return; // 超时，socket 在 onTimeout 中关闭
    }

    // 客户端关闭了连接
    if (ec == http::error::end_of_stream) {
        LOG_INFO("Client closed connection");
        deadline_.cancel();
        // 不需要调用 closeSocket，因为对端已经关闭了连接
        return;
    }

    if (ec) {
        LOG_WARN("Read error: {}", ec.message());
        deadline_.cancel();
        closeSocket("Read error");
        return;
    }

    boost::ignore_unused(bytes_transferred);

    // 处理请求
    handleRequest();

    if (!socket_.is_open()) {
        deadline_.cancel();
        return; // 套接字已经关闭
    }

    // 发送响应
    doWrite();
}

void HttpConnection::doWrite() {
    bool keep_alive = res_.keep_alive();

    res_.content_length(res_.body().size());
    res_.prepare_payload();

    resetDeadline();

    auto self = shared_from_this();
    http::async_write(socket_, res_, [self, keep_alive](beast::error_code ec, std::size_t bytes_transferred) {
        self->onWrite(ec, bytes_transferred, keep_alive);
    });
}

void HttpConnection::onWrite(beast::error_code ec, std::size_t bytes_transferred, bool keep_alive) {
    if (ec == asio::error::operation_aborted) {
        LOG_WARN("Write operation aborted");
        return; // 超时，socket 在 onTimeout 中关闭
    }

    if (ec) {
        LOG_WARN("Write error: {}", ec.message());
        deadline_.cancel();
        closeSocket("Write error");
        return;
    }

    boost::ignore_unused(bytes_transferred);

    if (keep_alive) {
        // 如果需要保持连接，继续读取下一个请求
        doRead();
    } else {
        // 关闭连接
        deadline_.cancel();
        closeSocket("Connection closed");
    }
}

// 处理收到的请求
void HttpConnection::handleRequest() {
    res_.keep_alive(false);

    res_.version(req_.version());
    res_.set(http::field::server, "GateServer");

    if (req_.method() == http::verb::get) {
        preParseGetParams(); // 预解析 GET 请求参数
        bool success = LogicSystem::getInstance()->handleGetRequest(get_url_, shared_from_this());
        if (!success) {
            res_.result(http::status::not_found);
            res_.set(http::field::content_type, "text/plain");
            beast::ostream(res_.body()) << "Resource not found\r\n";
            LOG_WARN("From {} {} {} {} {}", this->socket_.remote_endpoint().address().to_string(), req_.method_string(),
                     req_.target(), req_.version(), res_.result_int());
            return;
        }

        res_.result(http::status::ok);
        if (!res_.has_content_length() && res_.body().size() > 0 && !res_.count(http::field::content_type)) {
            res_.set(http::field::content_type, "text/plain");
        }

        LOG_INFO("From {} {} {} {} {}", this->socket_.remote_endpoint().address().to_string(), req_.method_string(),
                 req_.target(), req_.version(), res_.result_int());
    } else if (req_.method() == http::verb::post) {
        // 处理 POST 请求
        bool success = LogicSystem::getInstance()->handlePostRequest(req_.target(), shared_from_this());
        if (!success) {
            res_.result(http::status::not_found);
            res_.set(http::field::content_type, "text/plain");
            beast::ostream(res_.body()) << "Resource not found\r\n";
            LOG_WARN("From {} {} {} {} {}", this->socket_.remote_endpoint().address().to_string(), req_.method_string(),
                     req_.target(), req_.version(), res_.result_int());
            return;
        }

        res_.result(http::status::ok);
        if (!res_.has_content_length() && res_.body().size() > 0 && !res_.count(http::field::content_type)) {
            res_.set(http::field::content_type, "application/json");
        }
        LOG_INFO("From {} {} {} {} {}", this->socket_.remote_endpoint().address().to_string(), req_.method_string(),
                 req_.target(), req_.version(), res_.result_int());
    } else {
        res_.result(http::status::method_not_allowed);
        res_.set(http::field::content_type, "text/plain");
        res_.set(http::field::allow, "GET");
        beast::ostream(res_.body()) << "Method not allowed\r\n";
        LOG_WARN("From {} {} {} {} {}", this->socket_.remote_endpoint().address().to_string(), req_.method_string(),
                 req_.target(), req_.version(), res_.result_int());
    }
}

// 检查超时
void HttpConnection::onTimeout(beast::error_code ec) {
    if (ec == asio::error::operation_aborted) {
        return; // 超时操作被取消
    }

    // 定时器发生错误
    if (ec) {
        LOG_WARN("Timeout error: {}", ec.message());
        return;
    }

    LOG_DEBUG("Idle timeout, closing socket");
    closeSocket("Idle timeout");
}

void HttpConnection::resetDeadline() {
    deadline_.expires_after(timeout_duration_);

    auto self = shared_from_this();
    deadline_.async_wait([self](beast::error_code ec) {
        self->onTimeout(ec);
    });
}

void HttpConnection::closeSocket(const std::string &reason) {
    if (!socket_.is_open()) {
        return; // 套接字已经关闭
    }
    beast::error_code ec;
    socket_.shutdown(tcp::socket::shutdown_both, ec);   // 忽略关闭错误，无论如何都会关闭
    socket_.close(ec); // 关闭套接字
    if (ec) {
        LOG_WARN("Socket close error: {}", ec.message());
    }

    deadline_.cancel(); // 取消定时器
}

/**
 * @brief 预解析 GET 请求参数，将查询参数存储在 get_params_ 中
 * @return void
 */
void HttpConnection::preParseGetParams() {
    std::string url = req_.target();
    size_t query_pos = url.find('?');
    if (query_pos == std::string::npos) {
        get_url_ = url;
        return; // 没有查询参数
    }

    get_url_ = url.substr(0, query_pos); // 获取 URL 部分
    std::string query_string = url.substr(query_pos + 1); // 获取查询参数部分
    std::string key, value;
    size_t pos = 0;
    while ((pos = query_string.find('&')) != std::string::npos) {
        std::string pair = query_string.substr(0, pos);
        size_t equal_pos = pair.find('=');
        if (equal_pos != std::string::npos) {
            key = pair.substr(0, equal_pos);
            value = pair.substr(equal_pos + 1);
            get_params_[urlDecode(key)] = urlDecode(value); // 解码参数
        }
        query_string.erase(0, pos + 1); // 删除已处理的部分
    }

    // 处理最后一个参数
    if (!query_string.empty()) {
        size_t equal_pos = query_string.find('=');
        if (equal_pos != std::string::npos) {
            key = query_string.substr(0, equal_pos);
            value = query_string.substr(equal_pos + 1);
            get_params_[urlDecode(key)] = urlDecode(value); // 解码参数
        }
    }
}
