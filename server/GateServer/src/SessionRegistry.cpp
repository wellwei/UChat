#include "SessionRegistry.h"
#include "ChatSession.h"
#include "Logger.h"

#if defined(__GNUC__)
__attribute__((noinline))
#endif

void SessionRegistry::Bind(int64_t uid, const std::shared_ptr<ChatSession>& session) {
    std::unique_lock<std::shared_mutex> lock(mu_);
    auto it = sessions_.find(uid);
    if (it != sessions_.end() && it->second != session) {
        // Kick the old session before replacing
        auto old_session = it->second;
        it->second = session;
        lock.unlock();

        old_session->KickAndFinish("Another device logged in");
        LOG_INFO("SessionRegistry: kicked old session for uid={}", uid);
    } else {
        sessions_[uid] = session;
    }
    LOG_DEBUG("SessionRegistry: bound uid={}", uid);
}

void SessionRegistry::Unbind(int64_t uid, int64_t session_ver) {
    std::unique_lock<std::shared_mutex> lock(mu_);
    auto it = sessions_.find(uid);
    if (it != sessions_.end() && it->second->GetSessionVer() == session_ver) {
        sessions_.erase(it);
        LOG_DEBUG("SessionRegistry: unbound uid={}", uid);
    }
}

std::shared_ptr<ChatSession> SessionRegistry::Find(int64_t uid) {
    std::shared_lock lock(mu_);
    auto it = sessions_.find(uid);
    return it != sessions_.end() ? it->second : nullptr;
}

std::vector<std::pair<int64_t, int64_t> > SessionRegistry::GetAllSessionsPair() {
    std::vector<std::pair<int64_t, int64_t> > sessions_pairs;

    for (const auto&[fst, snd] : sessions_) {
        std::lock_guard<std::shared_mutex> lock(mu_);
        sessions_pairs.emplace_back(fst, snd->GetSessionVer());
    }

    return sessions_pairs;
}

