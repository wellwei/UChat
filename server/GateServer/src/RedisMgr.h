//
// Created by wellwei on 2025/4/9.
//

#ifndef GATESERVER_REDISMGR_H
#define GATESERVER_REDISMGR_H

#include "global.h"
#include "RedisConnPool.h"
#include "Singleton.h"

class RedisMgr : public Singleton<RedisMgr> {
    friend class Singleton<RedisMgr>;

public:
    ~RedisMgr();

    std::optional<std::string> get(const std::string &key);

    bool set(const std::string &key, const std::string &value, int ttl_seconds = -1);

    bool del(const std::string &key);

    bool exists(const std::string &key);

    bool expire(const std::string &key, int ttl_seconds);

    bool hset(const std::string &key, const std::string &field, const std::string &value);

    std::optional<std::string> hget(const std::string &key, const std::string &field);

    bool hdel(const std::string &key, const std::string &field);

    bool hexists(const std::string &key, const std::string &field);

    bool lpush(const std::string &key, const std::string &value);

    std::optional<std::string> lpop(const std::string &key);

    bool rpush(const std::string &key, const std::string &value);

    std::optional<std::string> rpop(const std::string &key);

    // Atomically deletes key only if its current value equals expected_value.
    // Uses a Lua script to avoid TOCTOU races.
    bool conditionalDel(const std::string &key, const std::string &expected_value);

    bool refreshOnlineSessionsPresence(std::vector<std::pair<int64_t, int64_t>> &online_sessions, std::string &gateway_id);

private:
    RedisMgr();

    std::unique_ptr<RedisConnPool> _redis_conn_pool; // Redis 连接池
};


#endif //GATESERVER_REDISMGR_H
