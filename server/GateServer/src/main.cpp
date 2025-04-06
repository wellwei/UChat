#include "global.h"
#include "CServer.h"
#include "message.grpc.pb.h"

int main() {
    auto g_config_mgr = *ConfigMgr::getInstance(); // 获取配置管理器实例
    try {
        unsigned short port = atoi(g_config_mgr["GateServer"]["port"].c_str()) ?
                              atoi(g_config_mgr["GateServer"]["port"].c_str()) : 8080; // 获取端口号，默认为 8080
        boost::asio::io_context ioc{1}; // 创建一个 io_context 对象
        // 设置信号处理器，捕获 SIGINT 和 SIGTERM 信号
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        // 如果捕获到信号，则停止 io_context
        signals.async_wait([&ioc](const boost::system::error_code &ec, int signal_number) {
            if (!ec) {
                std::cout << "Received signal " << signal_number << ", stopping server." << std::endl;
                ioc.stop();
            }
        });

        std::make_shared<CServer>(ioc, port)->start(); // 创建并启动服务器
        std::cout << "Server started on port " << port << std::endl;
        ioc.run(); // 运行 io_context，开始处理异步操作
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
