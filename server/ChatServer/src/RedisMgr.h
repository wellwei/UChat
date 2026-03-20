//
// ChatServer Redis Manager
//

#ifndef CHATSERVER_REDISMGR_H
#define CHATSERVER_REDISMGR_H

#include "RedisConnPool.h"
#include "Singleton.h"
#include "im.pb.h"

struct SyncBatchResult {
    std::vector<im::Envelope> messages;
    uint64_t max_inbox_seq = 0;
    bool has_more = false;
};

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

    // Offline inbox operations.
    // user_inbox_seq:{uid} -> next inbox sequence
    // offline_index:{uid} -> ZSET(score=inbox_seq, member=delivery_id)
    // offline_msg:{delivery_id} -> serialized Envelope
    uint64_t getNextInboxSeq(int64_t uid);
    bool appendOfflineMessage(int64_t uid, const im::Envelope& env);
    SyncBatchResult getOfflineMessagesBySeq(int64_t uid, uint64_t last_inbox_seq, uint32_t limit);
    bool ackInboxMessages(int64_t uid, uint64_t max_inbox_seq);

    uint64_t getAckedInboxSeq(int64_t uid);

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
