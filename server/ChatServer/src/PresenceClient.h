#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <memory>
#include <sw/redis++/redis++.h>

struct PresenceInfo {
    std::string gateway_id;
    int64_t session_ver = 0;
    std::string gateway_addr; // "host:port" of DeliverApi
};

// Wraps Redis queries for presence data and idempotency keys.
// Owns its own Redis connection pool (thread-safe internally).
class PresenceClient {
public:
    PresenceClient(const std::string& host, int port, const std::string& password,
                   size_t pool_size = 4);
    ~PresenceClient() = default;

    // Look up uid's live session. Returns nullopt if offline.
    std::optional<PresenceInfo> GetPresence(int64_t uid);

    // Idempotency: returns existing server_msg_id if key exists, else nullopt.
    std::optional<std::string> CheckIdempotent(int64_t from_uid, const std::string& client_msg_id);

    // Store idempotency result (TTL = 24h).
    void SetIdempotent(int64_t from_uid, const std::string& client_msg_id,
                       const std::string& server_msg_id);

private:
    std::unique_ptr<sw::redis::Redis> redis_;
};
