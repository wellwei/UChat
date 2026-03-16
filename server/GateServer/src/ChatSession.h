#pragma once

#include <grpcpp/grpcpp.h>
#include <mutex>
#include <deque>
#include <atomic>
#include <string>
#include <cstdint>
#include <map>
#include <chrono>
#include <thread>
#include <condition_variable>
#include "im.grpc.pb.h"

class SessionRegistry;

// In-flight message entry for tracking pushed but unacknowledged messages
// Note: This is for tracking/diagnostics only. We don't retry because:
// 1. Messages are persisted in Redis (offline queue)
// 2. Client will pull missed messages via Sync when reconnected
// 3. Inflight entries are auto-cleaned by timer after 5 seconds
struct InflightEntry {
    im::Envelope env;
    std::chrono::steady_clock::time_point push_time;

    // Auto-cleanup after 5 seconds using timer (not heartbeat-dependent)
    static constexpr int kAutoCleanupSec = 5;
};

// ServerBidiReactor for the ChatTunnel.Connect RPC.
// One instance per connected client. Heap-allocated; self-deletes in OnDone().
class ChatSession : public grpc::ServerBidiReactor<im::ClientFrame, im::ServerFrame>, public std::enable_shared_from_this<ChatSession> {
public:
    ChatSession(SessionRegistry* registry, const std::string& gateway_id);

    void SetSelf(std::shared_ptr<ChatSession> self) { self_ = std::move(self); }

    // Called when StartRead completes.
    void OnReadDone(bool ok) override;

    // Called when StartWrite completes.
    void OnWriteDone(bool ok) override;

    // Called once, after the stream is fully done. Self-deletes here.
    void OnDone() override;

    // Thread-safe: enqueue a frame to send to the client.
    void Enqueue(im::ServerFrame frame);

    // Called by SessionRegistry when a newer session displaces this one.
    void KickAndFinish(const std::string& reason);

    // Accessor for session version (used by SessionRegistry::Unbind).
    int64_t GetSessionVer() const { return session_ver_.load(std::memory_order_relaxed); }

    // In-flight message management (called by DeliverApiService)
    void TrackInflight(const im::Envelope& env);
    void AckInflight(const std::string& msg_id);

    void StartHeartbeatCheck();

private:
    void HandleLogin(const im::LoginReq& req);
    void HandleHeartbeat(const im::Heartbeat& req);
    void HandleSend(const im::SendReq& req);
    void HandleAck(const im::AckReq& req);

    // Must be called with mu_ held.
    void MaybeStartWrite();

    void UpdateActiveTime();
    void CheckHeartbeat();
    void CleanupPresence();

    std::shared_ptr<ChatSession> self_;
    SessionRegistry* registry_;
    std::string gateway_id_;
    int online_ttl_seconds_ = 15;

    std::atomic<int64_t> uid_ = 0;
    std::atomic<int64_t> session_ver_ = 0;
    std::atomic<bool> authed_ = false;

    std::mutex mu_;
    std::deque<im::ServerFrame> outbox_;
    std::atomic_bool write_inflight_ = false;
    std::atomic_bool finishing_ = false;

    im::ClientFrame in_frame_;

    // In-flight messages: msg_id -> InflightEntry
    std::map<std::string, InflightEntry> inflight_;

    // Last heartbeat timestamp for health check (stored as milliseconds since epoch)
    std::atomic<int64_t> last_active_time_ns{0};

    static constexpr size_t kMaxOutboxSize = 1000;
};
