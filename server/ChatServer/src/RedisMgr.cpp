//
// ChatServer Redis Manager
//

#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "Logger.h"
#include <sw/redis++/command_options.h>
#include <algorithm>
#include <cstdlib>
#include <functional>

namespace {

class ScopeExit {
public:
    explicit ScopeExit(std::function<void()> fn) : fn_(std::move(fn)) {}
    ~ScopeExit() {
        if (active_ && fn_) {
            fn_();
        }
    }

    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;

private:
    std::function<void()> fn_;
    bool active_ = true;
};

constexpr int MAX_OFFLINE_MSG = 1000;
constexpr int EXPIRE_SECONDS = 7 * 24 * 3600;
constexpr int ACK_EXPIRE_SECONDS = 7 * 24 * 3600;
constexpr int IDEMPOTENT_EXPIRE_SECONDS = 24 * 3600;

std::string InboxSeqKey(int64_t uid) {
    return "user_inbox_seq:" + std::to_string(uid);
}

std::string OfflineIndexKey(int64_t uid) {
    return "offline_index:" + std::to_string(uid);
}

std::string AckSeqKey(int64_t uid) {
    return "ack_seq:" + std::to_string(uid);
}

std::string DeliveryId(int64_t uid, uint64_t seq_id) {
    return std::to_string(uid) + ":" + std::to_string(seq_id);
}

std::string OfflineMsgKey(const std::string& delivery_id) {
    return "offline_msg:" + delivery_id;
}

uint32_t NormalizeSyncLimit(uint32_t limit) {
    if (limit == 0) {
        return 200;
    }
    return std::min<uint32_t>(limit, 500);
}

}  // namespace

RedisMgr::RedisMgr() {
    auto& config_mgr = ConfigMgr::getInstance();
    std::string host = config_mgr["Redis"].get("host", "127.0.0.1");
    if (const char* env_host = std::getenv("UCHAT_REDIS_HOST"); env_host && *env_host) {
        host = env_host;
    }

    int port = strtol(config_mgr["Redis"].get("port", "6379").c_str(), nullptr, 10);
    if (const char* env_port = std::getenv("UCHAT_REDIS_PORT"); env_port && *env_port) {
        port = strtol(env_port, nullptr, 10);
    }

    int pool_size = strtol(config_mgr["Redis"].get("pool_size", "16").c_str(), nullptr, 10);
    if (const char* env_pool = std::getenv("UCHAT_REDIS_POOL_SIZE"); env_pool && *env_pool) {
        pool_size = strtol(env_pool, nullptr, 10);
    }
    pool_size = std::max(pool_size, 1);
    _redis_conn_pool = std::make_unique<RedisConnPool>(static_cast<size_t>(pool_size), host, port);
    LOG_INFO("RedisMgr initialized host={} port={} pool_size={}", host, port, pool_size);
}

RedisMgr::~RedisMgr() {
    _redis_conn_pool->close();
    _redis_conn_pool.reset();
}

std::optional<std::string> RedisMgr::get(const std::string &key) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return std::nullopt;
    }
    ScopeExit guard([this, &conn]() {
        if (conn) {
            _redis_conn_pool->returnConnection(std::move(conn));
        }
    });

    try {
        auto value = conn->get(key);
        if (value) {
            return *value;
        }
        return std::nullopt;
    } catch (const std::exception &e) {
        LOG_ERROR("Redis get error: {}", e.what());
        return std::nullopt;
    }
}

bool RedisMgr::set(const std::string &key, const std::string &value, int ttl_seconds) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    ScopeExit guard([this, &conn]() {
        if (conn) {
            _redis_conn_pool->returnConnection(std::move(conn));
        }
    });

    try {
        if (ttl_seconds > 0) {
            conn->setex(key, ttl_seconds, value);
        } else {
            conn->set(key, value);
        }
        return true;
    } catch (const std::exception &e) {
        LOG_ERROR("Redis set error: {}", e.what());
        return false;
    }
}

bool RedisMgr::del(const std::string &key) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    ScopeExit guard([this, &conn]() {
        if (conn) {
            _redis_conn_pool->returnConnection(std::move(conn));
        }
    });

    try {
        conn->del(key);
        return true;
    } catch (const std::exception &e) {
        LOG_ERROR("Redis del error: {}", e.what());
        return false;
    }
}

bool RedisMgr::exists(const std::string &key) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    ScopeExit guard([this, &conn]() {
        if (conn) {
            _redis_conn_pool->returnConnection(std::move(conn));
        }
    });

    try {
        return conn->exists(key) > 0;
    } catch (const std::exception &e) {
        LOG_ERROR("Redis exists error: {}", e.what());
        return false;
    }
}

bool RedisMgr::expire(const std::string &key, int ttl_seconds) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    ScopeExit guard([this, &conn]() {
        if (conn) {
            _redis_conn_pool->returnConnection(std::move(conn));
        }
    });

    try {
        return conn->expire(key, ttl_seconds);
    } catch (const std::exception &e) {
        LOG_ERROR("Redis expire error: {}", e.what());
        return false;
    }
}

bool RedisMgr::rpush(const std::string &key, const std::string &value) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    ScopeExit guard([this, &conn]() {
        if (conn) {
            _redis_conn_pool->returnConnection(std::move(conn));
        }
    });

    try {
        conn->rpush(key, value);
        return true;
    } catch (const std::exception &e) {
        LOG_ERROR("Redis rpush error: {}", e.what());
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
    ScopeExit guard([this, &conn]() {
        if (conn) {
            _redis_conn_pool->returnConnection(std::move(conn));
        }
    });

    try {
        conn->lrange(key, start, stop, std::back_inserter(result));
        return result;
    } catch (const std::exception &e) {
        LOG_ERROR("Redis lrange error: {}", e.what());
        return result;
    }
}

uint64_t RedisMgr::getNextInboxSeq(int64_t uid) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return 0;
    }
    ScopeExit guard([this, &conn]() {
        if (conn) {
            _redis_conn_pool->returnConnection(std::move(conn));
        }
    });

    try {
        const auto seq = conn->incr(InboxSeqKey(uid));
        conn->expire(InboxSeqKey(uid), EXPIRE_SECONDS);
        return static_cast<uint64_t>(seq);
    } catch (const std::exception &e) {
        LOG_ERROR("RedisMgr::getNextInboxSeq error: {}", e.what());
        return 0;
    }
}

bool RedisMgr::appendOfflineMessage(int64_t uid, const im::Envelope& env) {
    if (uid <= 0 || env.seq_id() == 0 || env.msg_id().empty()) {
        LOG_ERROR("RedisMgr::appendOfflineMessage invalid params uid={} seq={} msg_id={}",
                  uid, env.seq_id(), env.msg_id());
        return false;
    }

    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    ScopeExit guard([this, &conn]() {
        if (conn) {
            _redis_conn_pool->returnConnection(std::move(conn));
        }
    });

    try {
        const std::string delivery_id = DeliveryId(uid, env.seq_id());
        const std::string msg_key = OfflineMsgKey(delivery_id);
        const std::string index_key = OfflineIndexKey(uid);
        const std::string payload = env.SerializeAsString();

        static const std::string script = R"lua(
            local msg_key = KEYS[1]
            local index_key = KEYS[2]
            local max_msgs = tonumber(ARGV[1])
            local expire_sec = tonumber(ARGV[2])
            local delivery_id = ARGV[3]
            local seq = tonumber(ARGV[4])
            local envelope = ARGV[5]

            redis.call('SETEX', msg_key, expire_sec, envelope)
            redis.call('ZADD', index_key, seq, delivery_id)
            redis.call('ZREMRANGEBYRANK', index_key, 0, -max_msgs - 1)
            redis.call('EXPIRE', index_key, expire_sec)
            return 1
        )lua";

        std::vector<std::string> keys = {msg_key, index_key};
        std::vector<std::string> args = {
            std::to_string(MAX_OFFLINE_MSG),
            std::to_string(EXPIRE_SECONDS),
            delivery_id,
            std::to_string(env.seq_id()),
            payload
        };

        conn->eval<long long>(script, keys.begin(), keys.end(), args.begin(), args.end());
        return true;
    } catch (const std::exception &e) {
        LOG_ERROR("RedisMgr::appendOfflineMessage error: {}", e.what());
        return false;
    }
}

SyncBatchResult RedisMgr::getOfflineMessagesBySeq(int64_t uid, uint64_t last_inbox_seq, uint32_t limit) {
    SyncBatchResult result;

    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return result;
    }
    ScopeExit guard([this, &conn]() {
        if (conn) {
            _redis_conn_pool->returnConnection(std::move(conn));
        }
    });

    try {
        const uint32_t fetch_limit = NormalizeSyncLimit(limit);
        const std::string index_key = OfflineIndexKey(uid);

        sw::redis::LimitOptions opts;
        opts.offset = 0;
        opts.count = static_cast<long long>(fetch_limit) + 1;

        std::vector<std::pair<std::string, double>> delivery_ids_with_score;
        conn->zrangebyscore(
            index_key,
            sw::redis::LeftBoundedInterval<double>(static_cast<double>(last_inbox_seq), sw::redis::BoundType::OPEN),
            opts,
            std::back_inserter(delivery_ids_with_score));

        if (delivery_ids_with_score.empty()) {
            return result;
        }

        if (delivery_ids_with_score.size() > fetch_limit) {
            result.has_more = true;
            delivery_ids_with_score.resize(fetch_limit);
        }

        std::vector<std::string> msg_keys;
        msg_keys.reserve(delivery_ids_with_score.size());
        for (const auto& entry : delivery_ids_with_score) {
            msg_keys.push_back(OfflineMsgKey(entry.first));
        }

        std::vector<sw::redis::OptionalString> raw_values;
        raw_values.reserve(msg_keys.size());
        conn->mget(msg_keys.begin(), msg_keys.end(), std::back_inserter(raw_values));

        for (std::size_t i = 0; i < raw_values.size(); ++i) {
            if (!raw_values[i]) {
                continue;
            }

            im::Envelope env;
            if (!env.ParseFromString(*raw_values[i])) {
                LOG_WARN("RedisMgr::getOfflineMessagesBySeq parse failed uid={} delivery_id={}",
                         uid, delivery_ids_with_score[i].first);
                continue;
            }

            result.max_inbox_seq = std::max(result.max_inbox_seq, env.seq_id());
            result.messages.push_back(std::move(env));
        }

        return result;
    } catch (const std::exception &e) {
        LOG_ERROR("RedisMgr::getOfflineMessagesBySeq error: {}", e.what());
        return result;
    }
}

bool RedisMgr::ackInboxMessages(int64_t uid, uint64_t max_inbox_seq) {
    if (uid <= 0 || max_inbox_seq == 0) {
        return true;
    }

    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return false;
    }
    ScopeExit guard([this, &conn]() {
        if (conn) {
            _redis_conn_pool->returnConnection(std::move(conn));
        }
    });

    try {
        uint64_t acked_seq = 0;
        auto ack_val = conn->get(AckSeqKey(uid));
        if (ack_val && !ack_val->empty()) {
            acked_seq = std::stoull(*ack_val);
        }
        if (max_inbox_seq <= acked_seq) {
            return true;
        }

        const std::string ack_key = AckSeqKey(uid);
        conn->setex(ack_key, ACK_EXPIRE_SECONDS, std::to_string(max_inbox_seq));
        return true;
    } catch (const std::exception &e) {
        LOG_ERROR("RedisMgr::ackInboxMessages error: {}", e.what());
        return false;
    }
}

uint64_t RedisMgr::getAckedInboxSeq(int64_t uid) {
    auto val = get(AckSeqKey(uid));
    if (!val || val->empty()) {
        return 0;
    }

    try {
        return std::stoull(*val);
    } catch (const std::exception& e) {
        LOG_ERROR("RedisMgr::getAckedInboxSeq error: {}", e.what());
        return 0;
    }
}

std::optional<std::string> RedisMgr::checkOrSetIdempotent(int64_t from_uid,
                                                           const std::string& client_msg_id,
                                                           const std::string& server_msg_id) {
    auto conn = _redis_conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection");
        return std::nullopt;
    }
    ScopeExit guard([this, &conn]() {
        if (conn) {
            _redis_conn_pool->returnConnection(std::move(conn));
        }
    });

    try {
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

        auto result = conn->eval<sw::redis::OptionalString>(script, keys.begin(), keys.end(), args.begin(), args.end());
        if (result) {
            return std::optional<std::string>{*result};
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        LOG_ERROR("RedisMgr::checkOrSetIdempotent error: {}", e.what());
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

std::optional<PresenceInfo> RedisMgr::getPresence(int64_t uid) {
    try {
        auto online_val = get("online:" + std::to_string(uid));
        if (!online_val) {
            return std::nullopt;
        }

        auto pos = online_val->rfind(':');
        if (pos == std::string::npos) {
            return std::nullopt;
        }

        std::string gateway_id = online_val->substr(0, pos);
        int64_t session_ver = std::stoll(online_val->substr(pos + 1));

        auto addr_val = get("gateway:" + gateway_id);
        if (!addr_val) {
            return std::nullopt;
        }

        return PresenceInfo{gateway_id, session_ver, *addr_val};
    } catch (const std::exception& e) {
        LOG_ERROR("RedisMgr::getPresence error: {}", e.what());
        return std::nullopt;
    }
}
