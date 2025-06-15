#include <iostream>
#include "Logger.h"
#include "MysqlMgr.h"
#include "ConfigMgr.h"
#include <cstdint>
#include "UserServer.h"
#include <csignal>
#include <thread>
#include "Auth.h"
#include "RedisMgr.h"

// 全局变量
UserServer* g_server = nullptr;

// 信号处理函数
void signalHandler(int signal) {
    if (g_server) {
        LOG_INFO("收到SIGINT信号，开始关闭...");
        g_server->stop();
        LOG_INFO("关闭成功");
        exit(0);
    }
}

int main() {
    // 初始化日志
    Logger::init();
    ConfigMgr::getInstance();
    MysqlMgr::getInstance();
    Auth::getInstance();
    RedisMgr::getInstance();

    UserServer user_server;
    g_server = &user_server;

    user_server.start();

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    return 0;
}