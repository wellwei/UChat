
#include "global.h"
#include "ConfigMgr.h"
#include "Logger.h"
#include "RedisMgr.h"
#include "ChatApiClient.h"
#include "UserServiceClient.h"
#include "VerifyGrpcClient.h"
#include "GatewayServer.h"
#include <csignal>
#include <memory>
#include <thread>

static std::shared_ptr<GatewayServer> g_gateway;
std::thread sig_thread;

static void StartSignalWaiter() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    // 必须：在创建其它线程/启动 gRPC 前屏蔽，让信号不进入异步 handler
    pthread_sigmask(SIG_BLOCK, &set, nullptr);
    sig_thread = std::thread([set]() mutable {
        int sig = 0;
        sigwait(&set, &sig);
        TimerWheel::getInstance()->Stop();
        if (g_gateway) g_gateway->Stop();
    });
}

int main() {
    // 设置信号处理器
    StartSignalWaiter();

    Logger::init("logs", "gateserver");
    Singleton<TimerWheel>::getInstance();

    try {
        Singleton<ConfigMgr>::getInstance();
        Singleton<RedisMgr>::getInstance();
        Singleton<ChatApiClient>::getInstance();
        Singleton<UserServiceClient>::getInstance();

        std::string verify_val = (*ConfigMgr::getInstance())["VerifyServer"].get("enabled", "true");
        bool verify_enabled = (verify_val != "false" && verify_val != "0");
        if (verify_enabled) {
            Singleton<VerifyGrpcClient>::getInstance();
        } else {
            LOG_INFO("VerifyServer disabled by config, skipping initialization");
        }
    } catch (...) {
        LOG_ERROR("服务初始化失败");
        return 1;
    }

    try {
        LOG_INFO("GateServer 启动中...");

        g_gateway = std::make_shared<GatewayServer>();
        g_gateway->Start();

        LOG_INFO("GateServer 启动成功，gRPC 网关监听中");
        g_gateway->Wait();  // 阻塞，直到 Stop() 被调用
    } catch (const std::exception& e) {
        LOG_ERROR("GateServer 启动失败：{}", e.what());
        return 1;
    } catch (...) {
        LOG_ERROR("未知错误");
        return 1;
    }

    LOG_INFO("GateServer 关闭");
    // 保证Gateway在日志系统之前退出
    if (sig_thread.joinable()) sig_thread.join();
    return 0;
}
