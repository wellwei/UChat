//
// Created by wellwei on 2025/4/9.
//

#include "RedisConnPool.h"
#include "Logger.h"
#include <utility>

RedisConnPool::RedisConnPool(size_t pool_size, std::string host, int port, const std::string &password)
        : _pool_size(pool_size),
          _host(std::move(host)),
          _port(port),
          _stop(false) {
    try {
        for (size_t i = 0; i < _pool_size; ++i) {
            auto conn = std::make_shared<sw::redis::Redis>("tcp://" + _host + ":" + std::to_string(_port));
            conn->auth(password);
            _connections.push(std::move(conn));
        }
        LOG_INFO("Redis connection pool created with size: {}", _pool_size);
    }
    catch (const sw::redis::Error &e) {
        LOG_CRITICAL("Error creating Redis connection pool: {}", e.what());
        throw;
    }
    catch (const std::exception &e) {
        LOG_CRITICAL("Error creating Redis connection pool: {}", e.what());
        throw;
    }
    catch (...) {
        LOG_CRITICAL("Unknown error creating Redis connection pool");
        throw;
    }
}

RedisConnPool::~RedisConnPool() {
    std::unique_lock<std::mutex> lock(_mutex);
    close();
    _connections = std::queue<std::shared_ptr<sw::redis::Redis>>();   // 清空连接队列
}

std::shared_ptr<sw::redis::Redis> RedisConnPool::getConnection() {
    std::unique_lock<std::mutex> lock(_mutex);
    _cond_var.wait(lock, [this] {
        return !_connections.empty() || _stop;
    });
    if (_stop) {
        return nullptr; // 如果停止了，返回空指针
    }

    auto conn = std::move(_connections.front());
    _connections.pop();
    return conn; // 返回连接
}

void RedisConnPool::returnConnection(std::shared_ptr<sw::redis::Redis> conn) {
    if (!conn) {
        return;
    }
    std::unique_lock<std::mutex> lock(_mutex);
    if (_stop) {
        return; // 如果停止了，直接返回
    }

    _connections.push(std::move(conn)); // 将连接放回队列
    _cond_var.notify_one(); // 通知一个等待的线程
}

void RedisConnPool::close() {
    _stop = true;
    _cond_var.notify_all(); // 通知所有等待的线程
}
