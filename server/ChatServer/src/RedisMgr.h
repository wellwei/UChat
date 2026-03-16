//
// ChatServer Redis Manager
//

#ifndef CHATSERVER_REDISMGR_H
#define CHATSERVER_REDISMGR_H

#include "RedisConnPool.h"
#include "Singleton.h"

struct PresenceInfo {
    std::string gateway_id;
    int64_t session_ver = 0;
    std::string gateway_addr; // "host:port" of DeliverApi
};

class RedisMgr : public Singleton<RedisMgr> {
    friend class Singleton<RedisMgr>;

public:
    ~RedisMgr();

    // String operations
    std::optional<std::string> get(const std::string &key);
    bool set(const std::string &key, const std::string &value, int ttl_seconds = -1);
    bool del(const std::string &key);
    bool exists(const std::string &key);
    bool expire(const std::string &key, int ttl_seconds);

    // List operations
    bool rpush(const std::string &key, const std::string &value);
    std::vector<std::string> lrange(const std::string &key, int64_t start, int64_t stop);

    // Offline message queue operations (7 days TTL, ZSet + Hash)
    // Hash: msg_content:{msg_id} -> message fields
    // ZSet: offline_index:{receiver_uid} -> score=timestamp, member=msg_id
    void addToOfflineQueue(int64_t to_uid, int64_t from_uid, const std::string& msg_id,
                           int64_t ts_ms, const std::string& payload, uint64_t seq_id = 0);
    std::vector<std::string> getOfflineMessages(int64_t to_uid);

    // Seq generator for message ordering
    // 单聊会话ID: min(uid1, uid2) + "_" + max(uid1, uid2)
    // 群聊会话ID: "group_" + group_id
    uint64_t getNextSeq(const std::string& conversation_id);
    uint64_t getMaxSeq(const std::string& conversation_id);

    // Idempotency operations
    // Atomic check-and-set: returns existing value if key exists, otherwise sets new value and returns nullopt
    std::optional<std::string> checkOrSetIdempotent(int64_t from_uid, const std::string& client_msg_id,
                                                     const std::string& server_msg_id);
    // Legacy methods (non-atomic, kept for backward compatibility)
    std::optional<std::string> checkIdempotent(int64_t from_uid, const std::string& client_msg_id);
    void setIdempotent(int64_t from_uid, const std::string& client_msg_id, const std::string& server_msg_id);

    // Presence operations
    std::optional<PresenceInfo> getPresence(int64_t uid);

private:
    RedisMgr();

    std::unique_ptr<RedisConnPool> _redis_conn_pool;
};


#endif //CHATSERVER_REDISMGR_H
