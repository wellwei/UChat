#include <iostream>
#include <memory>
#include "ConfigMgr.h"
#include "Logger.h"
#include "NewChatServer.h"
#include "RedisMgr.h"

int main() {
    try {
        Logger::init();
        LOG_INFO("ChatServer 启动中...");

        ConfigMgr::getInstance();
        RedisMgr::getInstance();

        // 创建并启动 NewChatServer
        auto chat_server = std::make_unique<NewChatServer>();
        chat_server->Start();

        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("启动异常: {}", e.what());
        return 1;
    }
}
