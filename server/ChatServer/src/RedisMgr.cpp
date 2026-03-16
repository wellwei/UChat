//
// ChatServer Redis Manager
//

#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "Logger.h"
#include <nlohmann/json.hpp>

RedisMgr::RedisMgr() {
    auto& config_mgr = ConfigMgr::getInstance();
    std::string host = config_mgr["Redis"]["host"];
    int port = strtol(config_mgr["Redis"]["port"].c_str(), nullptr, 10);
    _redis_conn_pool = std::make_unique<RedisConnPool>(10, host, port);
    LOG_INFO("RedisMgr initialized");
}

RedisMgr::~RedisMgr() {
    _redis_conn_pool->close();
    _redis_conn_pool.reset();
}

// String operations
std::optional<std::string> RedisMgr::get(const std::string &key) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return std::nullopt;
    }
    try {
        auto value = conn->get(key);
        _redis_conn_pool->returnConnection(std::move(conn));
        if (value) return *value;
        return std::nullopt;
    } catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis get error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
        return std::nullopt;
    }
}

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
    } catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis set error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
        return false;
    }
}

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
    } catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis del error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
        return false;
    }
}

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
    } catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis exists error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
        return false;
    }
}

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
    } catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis expire error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
        return false;
    }
}

// List operations
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
    } catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis rpush error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
        return false;
    }
}

std::vector<std::string> RedisMgr::lrange(const std::string &key, int64_t start, int64_t stop) {
    std::vector<std::string> result;
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return result;
    }
    try {
        conn->lrange(key, start, stop, std::back_inserter(result));
        _redis_conn_pool->returnConnection(std::move(conn));
        return result;
    } catch (const sw::redis::Error &e) {
        LOG_ERROR("Redis lrange error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
        return result;
    }
}

// ========== Offline message queue (7 days TTL, ZSet + Hash) ==========
// Hash: msg_content:{msg_id} -> message fields
// ZSet: offline_index:{receiver_uid} -> score=timestamp, member=msg_id
static const int MAX_OFFLINE_MSG = 1000;  // 每人最多 1000 条
static const int EXPIRE_SECONDS = 7 * 24 * 3600;  // 7 天
static const int SEQ_EXPIRE_SECONDS = 30 * 24 * 3600;  // 30 天
static const int IDEMPOTENT_EXPIRE_SECONDS = 24 * 3600;  // 24 小时

void RedisMgr::addToOfflineQueue(int64_t to_uid, int64_t from_uid,
                                  const std::string& msg_id, int64_t ts_ms,
                                  const std::string& payload, uint64_t seq_id) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return;
    }
    try {
        // 使用 Lua 脚本原子化所有操作
        static const std::string script = R"lua(
            local msg_key = KEYS[1]
            local index_key = KEYS[2]
            local max_msgs = tonumber(ARGV[1])
            local expire_sec = tonumber(ARGV[2])
            local msg_id = ARGV[3]
            local score = tonumber(ARGV[4])
            local sender_uid = ARGV[5]
            local receiver_uid = ARGV[6]
            local content = ARGV[7]
            local msg_type = ARGV[8]
            local create_time = ARGV[9]
            local seq_id = ARGV[10]

            -- 1. 存储消息内容 (Hash)
            redis.call('HSET', msg_key, 'sender_uid', sender_uid)
            redis.call('HSET', msg_key, 'receiver_uid', receiver_uid)
            redis.call('HSET', msg_key, 'content', content)
            redis.call('HSET', msg_key, 'msg_type', msg_type)
            redis.call('HSET', msg_key, 'create_time', create_time)
            if seq_id ~= '0' then
                redis.call('HSET', msg_key, 'seq_id', seq_id)
            end
            redis.call('EXPIRE', msg_key, expire_sec)

            -- 2. 添加到离线索引 (ZSet)
            redis.call('ZADD', index_key, score, msg_id)

            -- 3. 限制消息数量 (保留最新的 max_msgs 条)
            redis.call('ZREMRANGEBYRANK', index_key, 0, -max_msgs - 1)

            -- 4. 设置索引过期时间
            redis.call('EXPIRE', index_key, expire_sec)

            return 1
        )lua";

        std::string msg_key = "msg_content:" + msg_id;
        std::string index_key = "offline_index:" + std::to_string(to_uid);

        std::vector<std::string> keys = {msg_key, index_key};
        std::vector<std::string> args = {
            std::to_string(MAX_OFFLINE_MSG),
            std::to_string(EXPIRE_SECONDS),
            msg_id,
            std::to_string(ts_ms),
            std::to_string(from_uid),
            std::to_string(to_uid),
            payload,
            "1",  // msg_type
            std::to_string(ts_ms),
            std::to_string(seq_id)
        };

        conn->eval<long long>(script, keys.begin(), keys.end(), args.begin(), args.end());
        _redis_conn_pool->returnConnection(std::move(conn));
        LOG_DEBUG("RedisMgr::addToOfflineQueue: added msg_id={} for uid={}, seq_id={}", msg_id, to_uid, seq_id);
    } catch (const sw::redis::Error& e) {
        LOG_ERROR("RedisMgr::addToOfflineQueue error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
    }
}

std::vector<std::string> RedisMgr::getOfflineMessages(int64_t to_uid) {
    std::vector<std::string> result;
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return result;
    }
    try {
        std::string index_key = "offline_index:" + std::to_string(to_uid);

        // 1. 查询 ZSet 中所有消息 ID (按 score 升序)
        std::vector<std::string> msg_ids;
        conn->zrange(index_key, 0, -1, std::back_inserter(msg_ids));

        if (msg_ids.empty()) {
            _redis_conn_pool->returnConnection(std::move(conn));
            return result;
        }

        // 2. 获取每条消息内容 (Hash)
        for (const auto& msg_id : msg_ids) {
            std::string msg_key = "msg_content:" + msg_id;
            auto sender_uid = conn->hget(msg_key, "sender_uid");
            auto receiver_uid = conn->hget(msg_key, "receiver_uid");
            auto content = conn->hget(msg_key, "content");
            auto msg_type = conn->hget(msg_key, "msg_type");
            auto create_time = conn->hget(msg_key, "create_time");
            auto seq_id = conn->hget(msg_key, "seq_id");

            if (content) {
                // 转换为 JSON 格式
                nlohmann::json j;
                j["msg_id"] = msg_id;
                if (sender_uid) j["from_uid"] = std::stoll(*sender_uid);
                if (receiver_uid) j["to_uid"] = std::stoll(*receiver_uid);
                if (content) j["payload"] = *content;
                if (msg_type) j["msg_type"] = std::stoi(*msg_type);
                if (create_time) j["ts_ms"] = std::stoll(*create_time);
                if (seq_id) j["seq_id"] = std::stoull(*seq_id);
                result.push_back(j.dump());
            }
        }

        _redis_conn_pool->returnConnection(std::move(conn));
        LOG_DEBUG("RedisMgr::getOfflineMessages: got {} msgs for uid={}", result.size(), to_uid);
    } catch (const sw::redis::Error& e) {
        LOG_ERROR("RedisMgr::getOfflineMessages error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
    }
    return result;
}

// ========== Seq generator ==========

// 原子自增获取最新会话 SeqID（INCR + EXPIRE 原子化）
uint64_t RedisMgr::getNextSeq(const std::string& conversation_id) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return 0;
    }
    try {
        // 使用 Lua 脚本原子化 INCR + EXPIRE
        static const std::string script = R"lua(
            local key = KEYS[1]
            local expire_sec = tonumber(ARGV[1])
            local result = redis.call('INCR', key)
            redis.call('EXPIRE', key, expire_sec)
            return result
        )lua";

        std::string key = "msg_seq:" + conversation_id;
        std::vector<std::string> keys = {key};
        std::vector<std::string> args = {std::to_string(SEQ_EXPIRE_SECONDS)};

        auto result = conn->eval<long long>(script, keys.begin(), keys.end(), args.begin(), args.end());
        _redis_conn_pool->returnConnection(std::move(conn));
        return static_cast<uint64_t>(result);
    } catch (const sw::redis::Error& e) {
        LOG_ERROR("RedisMgr::getNextSeq error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
        return 0;
    }
}

uint64_t RedisMgr::getMaxSeq(const std::string& conversation_id) {
    std::string key = "msg_seq:" + conversation_id;
    auto val = get(key);
    if (val && !val->empty()) {
        return std::stoull(*val);
    }
    return 0;
}

// ========== Idempotency operations ==========

// 原子的幂等性检查和设置
// 返回：如果键已存在，返回已有值；如果键不存在，设置新值并返回 nullopt
std::optional<std::string> RedisMgr::checkOrSetIdempotent(int64_t from_uid,
                                                           const std::string& client_msg_id,
                                                           const std::string& server_msg_id) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return std::nullopt;
    }
    try {
        // 使用 Lua 脚本原子化 SETNX + GET
        static const std::string script = R"lua(
            local key = KEYS[1]
            local value = ARGV[1]
            local ttl = tonumber(ARGV[2])

            local existing = redis.call('GET', key)
            if existing then
                return existing
            end

            redis.call('SET', key, value, 'EX', ttl)
            return nil
        )lua";

        std::string key = "idempotent:" + std::to_string(from_uid) + ":" + client_msg_id;
        std::vector<std::string> keys = {key};
        std::vector<std::string> args = {server_msg_id, std::to_string(IDEMPOTENT_EXPIRE_SECONDS)};

        auto result = conn->eval<sw::redis::Optional<std::string>>(script, keys.begin(), keys.end(), args.begin(), args.end());
        _redis_conn_pool->returnConnection(std::move(conn));

        if (result) {
            LOG_DEBUG("{}", *result);
            return std::optional<std::string> {*result};
        } else {
            return std::nullopt;
        }

    } catch (const sw::redis::Error& e) {
        LOG_ERROR("RedisMgr::checkOrSetIdempotent error: {}", e.what());
        _redis_conn_pool->returnConnection(std::move(conn));
        return std::nullopt;
    }
}

std::optional<std::string> RedisMgr::checkIdempotent(int64_t from_uid, const std::string& client_msg_id) {
    std::string key = "idempotent:" + std::to_string(from_uid) + ":" + client_msg_id;
    return get(key);
}

void RedisMgr::setIdempotent(int64_t from_uid, const std::string& client_msg_id, const std::string& server_msg_id) {
    std::string key = "idempotent:" + std::to_string(from_uid) + ":" + client_msg_id;
    set(key, server_msg_id, IDEMPOTENT_EXPIRE_SECONDS);
}

// ========== Presence operations ==========

std::optional<PresenceInfo> RedisMgr::getPresence(int64_t uid) {
    try {
        auto online_val = get("online:" + std::to_string(uid));
        if (!online_val) return std::nullopt;

        // Format: "gateway_id:session_ver"
        auto pos = online_val->rfind(':');
        if (pos == std::string::npos) return std::nullopt;

        std::string gateway_id = online_val->substr(0, pos);
        int64_t session_ver = std::stoll(online_val->substr(pos + 1));

        // Get gateway address from Redis
        auto addr_val = get("gateway:" + gateway_id);
        if (!addr_val) return std::nullopt;

        return PresenceInfo{gateway_id, session_ver, *addr_val};
    } catch (const std::exception& e) {
        LOG_ERROR("RedisMgr::getPresence error: {}", e.what());
        return std::nullopt;
    }
}
