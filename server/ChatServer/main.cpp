#include <iostream>
#include <memory>
#include <signal.h>
#include "ConfigMgr.h"
#include "Logger.h"
#include "NewChatServer.h"
#include "ServicePool.h"

int main() {
    try {
        Logger::init();
        LOG_INFO("ChatServer 启动中...");

        auto& config = ConfigMgr::getInstance();
        LOG_INFO("配置管理器已初始化");

        ServicePool::getInstance();
        LOG_INFO("ServicePool 已初始化");

        // 创建并启动 NewChatServer
        auto chat_server = std::make_unique<NewChatServer>();
        chat_server->Start();

        LOG_INFO("开始关闭 ServicePool...");
        ServicePool::getInstance().stop();
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("启动异常: {}", e.what());
        return 1;
    }
}
