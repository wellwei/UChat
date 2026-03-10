#include "PresenceClient.h"
#include "Logger.h"

PresenceClient::PresenceClient(const std::string& host, int port, const std::string& password,
                               size_t pool_size) {
    sw::redis::ConnectionOptions opts;
    opts.host = host;
    opts.port = port;
    if (!password.empty()) {
        opts.password = password;
    }

    sw::redis::ConnectionPoolOptions pool_opts;
    pool_opts.size = pool_size;

    redis_ = std::make_unique<sw::redis::Redis>(opts, pool_opts);
    LOG_INFO("PresenceClient initialized: {}:{}", host, port);
}

std::optional<PresenceInfo> PresenceClient::GetPresence(int64_t uid) {
    try {
        auto online_val = redis_->get("online:" + std::to_string(uid));
        if (!online_val) return std::nullopt;

        // Format: "gateway_id:session_ver"
        auto pos = online_val->rfind(':');
        if (pos == std::string::npos) return std::nullopt;

        std::string gateway_id = online_val->substr(0, pos);
        int64_t session_ver = std::stoll(online_val->substr(pos + 1));

        auto addr_val = redis_->get("gateway:" + gateway_id);
        if (!addr_val) return std::nullopt;

        return PresenceInfo{gateway_id, session_ver, *addr_val};
    } catch (const sw::redis::Error& e) {
        LOG_ERROR("PresenceClient::GetPresence error: {}", e.what());
        return std::nullopt;
    }
}

std::optional<std::string> PresenceClient::CheckIdempotent(int64_t from_uid,
                                                            const std::string& client_msg_id) {
    try {
        std::string key = "idempotent:" + std::to_string(from_uid) + ":" + client_msg_id;
        auto val = redis_->get(key);
        if (val) return *val;
        return std::nullopt;
    } catch (const sw::redis::Error& e) {
        LOG_ERROR("PresenceClient::CheckIdempotent error: {}", e.what());
        return std::nullopt;
    }
}

void PresenceClient::SetIdempotent(int64_t from_uid, const std::string& client_msg_id,
                                   const std::string& server_msg_id) {
    try {
        std::string key = "idempotent:" + std::to_string(from_uid) + ":" + client_msg_id;
        // TTL = 24h (86400 seconds)
        redis_->setex(key, 86400, server_msg_id);
    } catch (const sw::redis::Error& e) {
        LOG_ERROR("PresenceClient::SetIdempotent error: {}", e.what());
    }
}
