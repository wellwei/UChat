//
// Created by wellwei on 2025/4/1.
//

#include "CServer.h"
#include "HttpConnection.h"
#include "ServicePool.h"
#include "Logger.h"

CServer::CServer(boost::asio::io_context &ioc, unsigned short &port)
        : ioc_(ioc),
          acceptor_(ioc, tcp::endpoint(tcp::v4(), port)),
          socket_(ioc) {
}

void CServer::start() {
    auto self = shared_from_this();

    // 获取一个 io_context 实例并将新连接推送给它负责
    auto &ioc = ServicePool::getInstance()->getNextService();

    auto new_connection = std::make_shared<HttpConnection>(ioc);
    acceptor_.async_accept(new_connection->getSocket(), [self, new_connection](const boost::system::error_code &ec) {
        try {
            // 出错获取下一个连接
            if (ec) {
                self->start();
                return;
            }

            LOG_DEBUG("接收到新连接 {}", new_connection->getSocket().remote_endpoint().address().to_string());

            new_connection->start(); // 启动新连接

            // 继续接受下一个连接
            self->start();
        }
        catch (const std::exception &e) {
            LOG_WARN("接收连接失败：{}", e.what());
            self->start();
        }
    });
}


