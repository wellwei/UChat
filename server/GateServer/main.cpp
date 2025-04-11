#include "global.h"
#include "CServer.h"
#include "ServicePool.h"
#include "ConfigMgr.h"

int main() {
    auto g_config_mgr = *ConfigMgr::getInstance();
    try {
        unsigned short port = atoi(g_config_mgr["GateServer"]["port"].c_str()) ?
                              atoi(g_config_mgr["GateServer"]["port"].c_str()) : 8080; // 获取端口号，默认为 8080

        auto accept_ioc = boost::asio::io_context();

        // 设置信号处理器，捕获 SIGINT 和 SIGTERM 信号
        boost::asio::signal_set signals(accept_ioc, SIGINT, SIGTERM);
        // 如果捕获到信号，则停止 io_context
        signals.async_wait([&accept_ioc](const boost::system::error_code &ec, int signal_number) {
            if (!ec) {
                std::cout << "Received signal " << signal_number << ", stopping server." << std::endl;
                ServicePool::getInstance()->stop(); // 停止 ServicePool
                accept_ioc.stop(); // 停止 io_context
            }
        });

        std::make_shared<CServer>(accept_ioc, port)->start(); // 创建并启动服务器
        std::cout << "Server started on port " << port << std::endl;
        accept_ioc.run();
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}