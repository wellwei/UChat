//
// Created by wellwei on 2025/4/1.
//

#ifndef GATESERVER_HTTPCONNECTION_H
#define GATESERVER_HTTPCONNECTION_H

#include "global.h"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
    friend class LogicSystem;

public:
    explicit HttpConnection(tcp::socket socket);

    ~HttpConnection() = default;

    void start();

private:
    void onTimeout(beast::error_code ec);

    void resetDeadline();

    void doRead();  // 开始读取请求

    void onRead(beast::error_code ec, std::size_t bytes_transferred);   // 读取完成的回调函数

    void doWrite(); // 开始写入响应

    void onWrite(beast::error_code ec, std::size_t bytes_transferred, bool keep_alive);  // 写入完成的回调函数

    void handleRequest();   // 处理请求

    void preParseGetParams(); // 预解析 GET 请求参数

    void closeSocket(const std::string &reason);    // 关闭套接字

    tcp::socket socket_;
    beast::flat_buffer buffer_{8192};           // 动态缓冲区
    http::request<http::dynamic_body> req_;             // 请求体
    http::response<http::dynamic_body> res_;            // 响应体
    asio::steady_timer deadline_{socket_.get_executor()};
    std::chrono::seconds timeout_duration_{std::chrono::seconds(30)}; // 超时时间

    std::string get_url_;
    std::unordered_map<std::string, std::string> get_params_; // GET 请求参数
};


#endif //GATESERVER_HTTPCONNECTION_H
