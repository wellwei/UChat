//
// Created by wellwei on 2025/4/1.
//

#include <iostream>
#include "CServer.h"
#include "HttpConnection.h"

CServer::CServer(boost::asio::io_context &ioc, unsigned short &port)
        : ioc_(ioc),
          acceptor_(ioc, tcp::endpoint(tcp::v4(), port)),
          socket_(ioc) {
}

void CServer::start() {
    auto self = shared_from_this();
    acceptor_.async_accept(socket_, [self](beast::error_code ec) {
        try {
            if (ec) {   // 如果发生错误，放弃这个连接，继续等待下一个连接
                std::cerr << "Accept error: " << ec.message() << std::endl;

                self->start();
                return;
            }

            std::cout << "New connection from: " << self->socket_.remote_endpoint().address().to_string() << std::endl;

            std::make_shared<HttpConnection>(std::move(self->socket_))->start();

            self->start();  // 继续等待下一个连接
        }
        catch (const std::exception &e) {
            std::cerr << "Exception: " << e.what() << std::endl;
            self->start();
        }
    });
}


