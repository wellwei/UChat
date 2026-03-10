#pragma once

#include <grpcpp/grpcpp.h>
#include <mutex>
#include <deque>
#include <atomic>
#include <string>
#include <cstdint>
#include "im.grpc.pb.h"

class SessionRegistry;

// ServerBidiReactor for the ChatTunnel.Connect RPC.
// One instance per connected client. Heap-allocated; self-deletes in OnDone().
class ChatSession : public grpc::ServerBidiReactor<im::ClientFrame, im::ServerFrame>, public std::enable_shared_from_this<ChatSession> {
public:
    ChatSession(SessionRegistry* registry, const std::string& gateway_id, int online_ttl_seconds);

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
    int64_t GetSessionVer() const { return session_ver_.load(std::memory_order_acquire); }

private:
    void HandleLogin(const im::LoginReq& req);
    void HandleHeartbeat(const im::Heartbeat& req);
    void HandleSend(const im::SendReq& req);
    void HandleAck(const im::AckReq& req);
    void HandlePull(const im::PullHistoryReq& req);
    void HandleGetVerifyCode(const im::GetVerifyCodeReq& req);
    void HandleRegister(const im::RegisterReq& req);
    void HandleAuthLogin(const im::AuthLoginReq& req);
    void HandleResetPassword(const im::ResetPasswordReq& req);

    // Must be called with mu_ held.
    void MaybeStartWrite();

    void CleanupPresence();

    std::shared_ptr<ChatSession> self_;
    SessionRegistry* registry_;
    std::string gateway_id_;
    int online_ttl_seconds_;

    std::atomic<int64_t> uid_ = 0;
    std::atomic<int64_t> session_ver_ = 0;
    std::atomic<bool> authed_ = false;

    std::mutex mu_;
    std::deque<im::ServerFrame> outbox_;
    bool write_inflight_ = false;
    bool finishing_ = false;

    im::ClientFrame in_frame_;

    static constexpr size_t kMaxOutboxSize = 1000;
};
