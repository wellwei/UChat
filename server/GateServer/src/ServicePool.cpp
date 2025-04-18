//
// Created by wellwei on 2025/4/6.
//

#include "ServicePool.h"
#include "Logger.h"

ServicePool::ServicePool(size_t pool_size)
        : _services(pool_size),
          _work_guards(pool_size),
          _next_service_index(0) {
    for (size_t i = 0; i < pool_size; ++i) {
        _services[i] = std::make_shared<Service>();
        _work_guards[i] = std::make_unique<WorkGuard>(_services[i]->get_executor());
        _threads.emplace_back([this, i]() {
            try {
                _services[i]->run();
            } catch (const std::exception &e) {
                LOG_ERROR("Service thread {} encountered an error: {}", i, e.what());
            }
        });
    }
}

ServicePool::~ServicePool() {
    stop();
}

boost::asio::io_context &ServicePool::getNextService() {
    size_t index = _next_service_index++ % _services.size();
    return *_services[index];
}

void ServicePool::stop() {
    for (auto &guard: _work_guards) {
        guard->get_executor().context().stop();         // 关闭对应的 io_context
        guard->reset();
    }
    for (auto &thread: _threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

