//
// Created by wellwei on 2025/4/9.
//

#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "Logger.h"

RedisMgr::RedisMgr() {
    auto config_mgr = *ConfigMgr::getInstance();
    std::string host = config_mgr["Redis"]["host"];
    int port = strtol(config_mgr["Redis"]["port"].c_str(), nullptr, 10);
    _redis_conn_pool = std::make_unique<RedisConnPool>(8, host, port);
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
    try {
        auto value = conn->get(key);
        _redis_conn_pool->returnConnection(std::move(conn)); // 归还连接
        if (value) {
            return *value;
        } else {
            return std::nullopt;
        }
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis get error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
        return std::nullopt;
    }
}

// 设置 Redis String 键值
bool RedisMgr::set(const std::string &key, const std::string &value, int ttl_seconds) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    try {
        if (ttl_seconds > 0) {
            conn->setex(key, ttl_seconds, value);
        } else {
            conn->set(key, value);
        }
        _redis_conn_pool->returnConnection(std::move(conn));
        return true;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis set error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
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
    try {
        conn->del(key);
        _redis_conn_pool->returnConnection(std::move(conn));
        return true;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis del error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
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
    try {
        auto result = conn->exists(key);
        _redis_conn_pool->returnConnection(std::move(conn));
        return result;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis exists error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
        return false;
    }
}

// 设置 Redis 键值的过期时间
bool RedisMgr::expire(const std::string &key, int ttl_seconds) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    try {
        auto result = conn->expire(key, ttl_seconds);
        _redis_conn_pool->returnConnection(std::move(conn));
        return result;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis expire error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
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
    try {
        conn->hset(key, field, value);
        _redis_conn_pool->returnConnection(std::move(conn));
        return true;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis hset error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
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
    try {
        auto value = conn->hget(key, field);
        _redis_conn_pool->returnConnection(std::move(conn));
        if (value) {
            return *value;
        } else {
            return std::nullopt;
        }
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis hget error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
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
    try {
        conn->hdel(key, field);
        _redis_conn_pool->returnConnection(std::move(conn));
        return true;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis hdel error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
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
    try {
        auto result = conn->hexists(key, field);
        _redis_conn_pool->returnConnection(std::move(conn));
        return result;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis hexists error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
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
    try {
        conn->lpush(key, value);
        _redis_conn_pool->returnConnection(std::move(conn));
        return true;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis lpush error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
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
    try {
        auto value = conn->lpop(key);
        _redis_conn_pool->returnConnection(std::move(conn));
        if (value) {
            return *value;
        } else {
            return std::nullopt;
        }
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis lpop error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
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
    try {
        conn->rpush(key, value);
        _redis_conn_pool->returnConnection(std::move(conn));
        return true;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis rpush error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
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
    try {
        auto value = conn->rpop(key);
        _redis_conn_pool->returnConnection(std::move(conn));
        if (value) {
            return *value;
        } else {
            return std::nullopt;
        }
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis rpop error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
        return std::nullopt;
    }
}

// 原子比较并删除：仅当键的当前值等于 expected_value 时才删除
bool RedisMgr::conditionalDel(const std::string &key, const std::string &expected_value) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    try {
        static const std::string script =
            "if redis.call('get', KEYS[1]) == ARGV[1] then "
            "  return redis.call('del', KEYS[1]) "
            "else "
            "  return 0 "
            "end";
        std::vector<std::string> keys = {key};
        std::vector<std::string> args = {expected_value};
        auto result = conn->eval<long long>(script, keys.begin(), keys.end(), args.begin(), args.end());
        _redis_conn_pool->returnConnection(std::move(conn));
        return result > 0;
    }
    catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis conditionalDel error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
        return false;
    }
}

bool RedisMgr::refreshOnlineSessionsPresence(std::vector<std::pair<int64_t, int64_t>> &online_sessions, std::string &gateway_id) {

    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }

    try {
        auto pipline = conn->pipeline();
        for (auto [uid, session_ver] : online_sessions) {
            pipline.setex("online:" + std::to_string(uid), 15, gateway_id + ":" + std::to_string(session_ver));
        }
        pipline.exec();
        _redis_conn_pool->returnConnection(std::move(conn));
        return true;
    } catch (const sw::redis::Error &e) {
        _redis_conn_pool->returnConnection(std::move(conn));
        return false;
    }
}