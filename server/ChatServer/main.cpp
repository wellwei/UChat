#include <iostream>
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <signal.h>
#include "ConfigMgr.h"
#include "Logger.h"
#include "ChatServer.h"
#include "ServicePool.h"

namespace asio = boost::asio;

int main() {
    try {
        Logger::init();
        LOG_INFO("ChatServer 启动中...");

        auto &config = ConfigMgr::getInstance();
        LOG_INFO("配置管理器已初始化");

        ServicePool::getInstance();
        LOG_INFO("ServicePool 已初始化");

        uint16_t port = std::stoi(config["ChatServer"].get("port", "8080"));
        
        // 接收器上下文
        asio::io_context acceptor_context;
        
        // 设置退出信号处理
        asio::signal_set signals(acceptor_context, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code& error, int signal_number) {
            if (!error) {
                LOG_INFO("收到退出信号 {}, 开始停止服务...", signal_number);
                acceptor_context.stop();
            }
        });
        
        // 创建启动ChatServer
        const auto chat_server = std::make_shared<ChatServer>(acceptor_context, port);
        chat_server->start();

        std::thread acceptor_thread([&acceptor_context]() {
            acceptor_context.run();
        });
        LOG_INFO("开始接收连接");

        // 等待接收器线程结束
        acceptor_thread.join();

        LOG_INFO("开始关闭 ServicePool...");
        ServicePool::getInstance().stop();
        return 0;
    } catch (const std::exception &e) {
        LOG_ERROR("启动异常: {}", e.what());
        return 1;
    }
}