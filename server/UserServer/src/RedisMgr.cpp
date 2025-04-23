//
// Created by wellwei on 2025/4/9.
//

#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "Logger.h"
#include "RedisConnPool.h"
#include "util.h"

RedisMgr::RedisMgr() {
    auto &config_mgr = *ConfigMgr::getInstance();
    _poll_size = strtoul(config_mgr["Redis"]["pool_size"].c_str(), nullptr, 10);
    _ttl_seconds = strtoul(config_mgr["Redis"]["ttl_seconds"].c_str(), nullptr, 10);
    _pubsub_channel = config_mgr["Redis"]["pubsub_channel"];
    _db_index = strtol(config_mgr["Redis"]["db_index"].c_str(), nullptr, 10);

    std::string host = config_mgr["Redis"]["host"];
    long port = strtol(config_mgr["Redis"]["port"].c_str(), nullptr, 10);
    std::string password = config_mgr["Redis"]["password"];
    _redis_conn_pool = std::make_unique<RedisConnPool>(host, port, password, _db_index, _poll_size);
}

RedisMgr::~RedisMgr() {
    _redis_conn_pool->close();
    _redis_conn_pool.reset(); // 释放连接池
}

// 获取 Redis String 键值
std::optional<std::string> RedisMgr::get(const std::string &key) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return std::nullopt; // 返回空值
    }
    Defer defer([this, &conn]() {
        _redis_conn_pool->returnConnection(std::move(conn));
    });
    try {
        auto value = conn->get(key);
        if (value) {
            return *value;
        } else {
            return std::nullopt;
        }
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis get error: {}", e.what());
        return std::nullopt;
    }
}

// 设置 Redis String 键值
bool RedisMgr::set(const std::string &key, const std::string &value) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    Defer defer([this, &conn]() {
        _redis_conn_pool->returnConnection(std::move(conn));
    });
    try {
        conn->setex(key, _ttl_seconds, value);
        return true;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis set error: {}", e.what());
        return false;
    }
}

// 删除 Redis 键值
bool RedisMgr::del(const std::string &key) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    Defer defer([this, &conn]() {
        _redis_conn_pool->returnConnection(std::move(conn));
    });
    try {
        conn->del(key);
        return true;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis del error: {}", e.what());
        return false;
    }
}

// 检查 Redis 键值是否存在
bool RedisMgr::exists(const std::string &key) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    Defer defer([this, &conn]() {
        _redis_conn_pool->returnConnection(std::move(conn));
    });
    try {
        auto result = conn->exists(key);
        return result;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis exists error: {}", e.what());
        return false;
    }
}

// 设置 Redis 键值的过期时间
bool RedisMgr::expire(const std::string &key) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    Defer defer([this, &conn]() {
        _redis_conn_pool->returnConnection(std::move(conn));
    });
    try {
        auto result = conn->expire(key, _ttl_seconds);
        return result;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis expire error: {}", e.what());
        return false;
    }
}

// 设置 Redis Hash 键值
bool RedisMgr::hset(const std::string &key, const std::string &field, const std::string &value) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    Defer defer([this, &conn]() {
        _redis_conn_pool->returnConnection(std::move(conn));
    });
    try {
        conn->hset(key, field, value);
        return true;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis hset error: {}", e.what());
        return false;
    }
}

// 获取 Redis Hash 键值
std::optional<std::string> RedisMgr::hget(const std::string &key, const std::string &field) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return std::nullopt;
    }
    Defer([this, &conn]() {
        _redis_conn_pool->returnConnection(std::move(conn));
    });
    try {
        auto value = conn->hget(key, field);
        if (value) {
            return *value;
        } else {
            return std::nullopt;
        }
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis hget error: {}", e.what());
        return std::nullopt;
    }
}

// 删除 Redis Hash 键值
bool RedisMgr::hdel(const std::string &key, const std::string &field) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    Defer defer([this, &conn]() {
        _redis_conn_pool->returnConnection(std::move(conn));
    });
    try {
        conn->hdel(key, field);
        return true;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis hdel error: {}", e.what());
        return false;
    }
}

// 检查 Redis Hash 键值是否存在
bool RedisMgr::hexists(const std::string &key, const std::string &field) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    Defer defer([this, &conn]() {
        _redis_conn_pool->returnConnection(std::move(conn));
    });
    try {
        auto result = conn->hexists(key, field);
        return result;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis hexists error: {}", e.what());
        return false;
    }
}

// 向 Redis 列表左侧推送元素
bool RedisMgr::lpush(const std::string &key, const std::string &value) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    Defer defer([this, &conn]() {
        _redis_conn_pool->returnConnection(std::move(conn));
    });
    try {
        conn->lpush(key, value);
        return true;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis lpush error: {}", e.what());
        return false;
    }
}

// 从 Redis 列表左侧弹出元素
std::optional<std::string> RedisMgr::lpop(const std::string &key) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return std::nullopt;
    }
    Defer defer([this, &conn]() {
        _redis_conn_pool->returnConnection(std::move(conn));
    });
    try {
        auto value = conn->lpop(key);
        if (value) {
            return *value;
        } else {
            return std::nullopt;
        }
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis lpop error: {}", e.what());
        return std::nullopt;
    }
}

// 向 Redis 列表右侧推送元素
bool RedisMgr::rpush(const std::string &key, const std::string &value) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    Defer defer([this, &conn]() {
        _redis_conn_pool->returnConnection(std::move(conn));
    });
    try {
        conn->rpush(key, value);
        return true;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis rpush error: {}", e.what());
        return false;
    }
}

// 从 Redis 列表右侧弹出元素
std::optional<std::string> RedisMgr::rpop(const std::string &key) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return std::nullopt;
    }
    Defer defer([this, &conn]() {
        _redis_conn_pool->returnConnection(std::move(conn));
    }); 
    try {
        auto value = conn->rpop(key);
        if (value) {
            return *value;
        } else {
            return std::nullopt;
        }
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis rpop error: {}", e.what());
        return std::nullopt;
    }
}

// 发布 Redis 消息
bool RedisMgr::publish(const std::string &channel, const std::string &message) {
    std::string pub_channel = channel.empty() ? _pubsub_channel : channel;  // 如果channel为空，则使用默认频道
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    Defer defer([this, &conn]() {
        _redis_conn_pool->returnConnection(std::move(conn));
    });
    try {
        auto result = conn->publish(pub_channel, message);
        LOG_DEBUG("向{}频道发布消息: {}, result : {}", pub_channel, message, result);
        return result;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis PUBLISH 失败 to channel '{}': {}", pub_channel, e.what());
        return false;
    }
}   