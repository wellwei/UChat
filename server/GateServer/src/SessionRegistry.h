#pragma once

#include <unordered_map>
#include <shared_mutex>
#include <cstdint>

class ChatSession;

// Thread-safe mapping of uid -> ChatSession*.
// Bind() replaces any existing session for the same uid (sending a Kick first).
// Unbind() only removes the session if the session_ver matches (prevents race).
class SessionRegistry {
public:
    SessionRegistry() = default;
    ~SessionRegistry() = default;

    // Bind a new session for uid. If another session exists, kicks it first.
    void Bind(int64_t uid, const std::shared_ptr<ChatSession>& session);

    // Unbind only if session_ver matches the current session's ver.
    void Unbind(int64_t uid, int64_t session_ver);

    // Find the live session for uid. Returns nullptr if not present.
    std::shared_ptr<ChatSession> Find(int64_t uid);

private:
    std::shared_mutex mu_;
    std::unordered_map<int64_t, std::shared_ptr<ChatSession>> sessions_;
};
