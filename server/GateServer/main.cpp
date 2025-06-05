
#include "global.h"
#include "CServer.h"
#include "ServicePool.h"
#include "ConfigMgr.h"
#include "Logger.h"
#include "MysqlMgr.h"
#include "RedisMgr.h"
#include "LogicSystem.h"

int main() {
    // 设置日志目录和文件名
    Logger::init("logs", "gateserver");
    // 初始化服务
    try {
        Singleton<ConfigMgr>::getInstance();
        Singleton<ServicePool>::getInstance();
        Singleton<LogicSystem>::getInstance();
        Singleton<MysqlMgr>::getInstance();
        Singleton<RedisMgr>::getInstance();
    }
    catch (...) {
        LOG_ERROR("服务初始化失败");
        return 1;
    }

    try {
        LOG_INFO("GateServer 启动中...");

        auto g_config_mgr = *Singleton<ConfigMgr>::getInstance();
        unsigned short port = atoi(g_config_mgr["GateServer"]["port"].c_str()) ?
                              atoi(g_config_mgr["GateServer"]["port"].c_str()) : 8080; // 获取端口号，默认为 8080

        auto accept_ioc = boost::asio::io_context();

        // 设置信号处理器，捕获 SIGINT 和 SIGTERM 信号
        boost::asio::signal_set signals(accept_ioc, SIGINT, SIGTERM);
        // 如果捕获到信号，则停止 io_context
        signals.async_wait([&accept_ioc](const boost::system::error_code &ec, int signal_number) {
            if (!ec) {
                LOG_INFO("收到信号 {}，停止服务器", signal_number);
                ServicePool::getInstance()->stop(); // 停止 ServicePool
                accept_ioc.stop(); // 停止 io_context
            }
        });

        std::make_shared<CServer>(accept_ioc, port)->start(); // 创建并启动服务器
        LOG_INFO("GateServer 启动成功，监听端口 {}", port);
        accept_ioc.run();
    }
    catch (const std::exception &e) {
        LOG_ERROR("GateServer 启动失败：{}", e.what());
        return 1;
    }
    catch (...) {
        LOG_ERROR("未知错误");
        return 1;
    }

    LOG_INFO("GateServer 关闭");

    return 0;
}