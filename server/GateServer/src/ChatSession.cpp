#include "ChatSession.h"
#include "SessionRegistry.h"
#include "RedisMgr.h"
#include "ChatApiClient.h"
#include "ConfigMgr.h"
#include "Logger.h"
#include "VerifyGrpcClient.h"
#include "UserServiceClient.h"
#include "global.h"
#include <jwt-cpp/jwt.h>
#include <nlohmann/json.hpp>
#include <chrono>

#include "TimerWheel.h"

ChatSession::ChatSession(SessionRegistry* registry, const std::string& gateway_id)
    : registry_(registry), gateway_id_(gateway_id) {
    StartRead(&in_frame_);
}

void ChatSession::OnReadDone(bool ok) {
    if (!ok) {
        // Client disconnected or stream cancelled
        {
            std::lock_guard<std::mutex> lock(mu_);
            if (!finishing_) {
                finishing_ = true;
                Finish(grpc::Status::OK);
            }
        }
        return;
    }

    UpdateActiveTime();

    // ChatTunnel 只处理核心数据流操作
    // 其他控制操作已迁移到 ChatControlApi (unary RPC)
    switch (in_frame_.body_case()) {
        case im::ClientFrame::kLogin:
            HandleLogin(in_frame_.login());
            break;
        case im::ClientFrame::kHb:
            HandleHeartbeat(in_frame_.hb());
            break;
        case im::ClientFrame::kSend:
            HandleSend(in_frame_.send());
            break;
        case im::ClientFrame::kAck:
            HandleAck(in_frame_.ack());
            break;
        default:
            LOG_WARN("ChatSession: unknown or deprecated frame type from uid={}", uid_.load());
            break;
    }

    in_frame_.Clear();
    StartRead(&in_frame_);
}

void ChatSession::OnWriteDone(bool ok) {
    std::lock_guard lock(mu_);
    write_inflight_ = false;
    if (!ok) {
        if (!finishing_) {
            finishing_ = true;
            Finish(grpc::Status::OK);
        }
        return;
    }
    outbox_.pop_front();
    MaybeStartWrite();
}

void ChatSession::OnDone() {
    auto keep_alive = std::move(self_);

    if (authed_) {
        registry_->Unbind(uid_, session_ver_.load());
    }

    // Log unacknowledged messages for monitoring/debugging
    // Note: These messages are still in Redis (offline queue), so client will pull them later
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (!inflight_.empty()) {
            LOG_INFO("ChatSession::OnDone uid={}, session ended with {} unacknowledged messages (will be pulled by client later)",
                     uid_.load(), inflight_.size());
        }
    }

    LOG_INFO("ChatSession::OnDone uid={}", uid_.load());
    CleanupPresence();
}

void ChatSession::Enqueue(im::ServerFrame frame) {
    std::lock_guard lock(mu_);
    if (finishing_) return;
    if (outbox_.size() >= kMaxOutboxSize) {
        LOG_WARN("ChatSession: outbox full for uid={}, disconnecting", uid_.load());
        if (!finishing_) {
            finishing_ = true;
            Finish(grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "outbox full"));
        }
        return;
    }
    outbox_.push_back(std::move(frame));
    MaybeStartWrite();
}

void ChatSession::KickAndFinish(const std::string& reason) {
    im::ServerFrame kick_frame;
    kick_frame.mutable_kick()->set_reason(reason);
    Enqueue(std::move(kick_frame));
    std::lock_guard<std::mutex> lock(mu_);
    if (!finishing_) {
        finishing_ = true;
        // Finish will be called after the kick frame is sent, or immediately
        Finish(grpc::Status::OK);
    }
}

void ChatSession::MaybeStartWrite() {
    // Must be called with mu_ held
    if (write_inflight_ || outbox_.empty() || finishing_) return;
    write_inflight_ = true;
    StartWrite(&outbox_.front());
}

void ChatSession::CleanupPresence() {
    if (!authed_) return;
    std::string key = "online:" + std::to_string(uid_);
    std::string expected = gateway_id_ + ":" + std::to_string(session_ver_);
    RedisMgr::getInstance()->conditionalDel(key, expected);
    LOG_DEBUG("ChatSession: cleaned presence for uid={}", uid_.load());
}

void ChatSession::HandleLogin(const im::LoginReq& req) {
    if (authed_) {
        im::ServerFrame resp;
        resp.mutable_err()->set_code(400);
        resp.mutable_err()->set_message("Already authenticated");
        Enqueue(std::move(resp));
        return;
    }

    // Verify JWT
    try {
        auto& config = *ConfigMgr::getInstance();
        const auto decoded = jwt::decode(req.token());
        const auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256(config["JWT"]["secret_key"]))
            .with_issuer("UserService");
        verifier.verify(decoded);

        int64_t token_uid = std::stoll(decoded.get_subject());
        if (req.uid() != token_uid) {
            im::ServerFrame resp;
            auto* lr = resp.mutable_login();
            lr->set_ok(false);
            lr->set_reason("UID mismatch");
            Enqueue(std::move(resp));
            return;
        }
    } catch (const std::exception& e) {
        LOG_WARN("ChatSession: JWT verification failed: {}", e.what());
        im::ServerFrame resp;
        auto* lr = resp.mutable_login();
        lr->set_ok(false);
        lr->set_reason("Invalid or expired token");
        Enqueue(std::move(resp));
        return;
    }

    uid_ = req.uid();
    session_ver_.store(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count(), std::memory_order_release);
    authed_ = true;

    // Register in session registry
    registry_->Bind(uid_, shared_from_this());

    std::string key = "online:" + std::to_string(uid_);
    std::string expected = gateway_id_ + ":" + std::to_string(session_ver_);
    RedisMgr::getInstance()->set(key, expected, online_ttl_seconds_);

    StartHeartbeatCheck();

    im::ServerFrame resp;
    auto* lr = resp.mutable_login();
    lr->set_ok(true);
    lr->set_session_ver(session_ver_);
    Enqueue(std::move(resp));

    LOG_INFO("ChatSession: uid={} logged in, session_ver={}", uid_.load(), session_ver_.load());
}

void ChatSession::HandleHeartbeat(const im::Heartbeat& req) {
    int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    im::ServerFrame resp;
    resp.mutable_pong()->set_ts_ms(now_ms);
    Enqueue(std::move(resp));
}

void ChatSession::HandleSend(const im::SendReq& req) {
    if (!authed_) {
        im::ServerFrame resp;
        resp.mutable_err()->set_code(401);
        resp.mutable_err()->set_message("Not authenticated");
        Enqueue(std::move(resp));
        return;
    }

    im::SendMessageReq chat_req;
    chat_req.set_from_uid(uid_);
    chat_req.set_to_uid(req.env().to_uid());
    chat_req.set_group_id(req.env().group_id());
    chat_req.set_client_msg_id(req.env().client_msg_id());
    chat_req.set_payload(req.env().payload());
    chat_req.set_trace_id(req.env().trace_id());

    auto resp_proto = ChatApiClient::getInstance()->SendMessage(chat_req);

    im::ServerFrame frame;
    auto* send_resp = frame.mutable_send();
    send_resp->set_ok(resp_proto.ok());
    send_resp->set_msg_id(resp_proto.msg_id());
    if (!resp_proto.ok()) {
        send_resp->set_reason(resp_proto.reason());
    }
    Enqueue(std::move(frame));
}

void ChatSession::HandleAck(const im::AckReq& req) {
    if (!authed_) return;

    // Handle network-level ACK (ACK_RECEIVED): remove from inflight map
    // This indicates the client has received the message at network layer
    if (req.type() == im::ACK_RECEIVED) {
        AckInflight(req.msg_id());
        LOG_DEBUG("ChatSession::HandleAck: received network-level ACK for msg_id={}", req.msg_id());
    }

    // Forward all ACKs to ChatServer for potential business logic processing
    // (e.g., DELIVERED/READ state tracking in the future)
    im::AckUpReq ack_req;
    ack_req.set_uid(uid_);
    ack_req.set_msg_id(req.msg_id());
    ack_req.set_type(req.type());
    ack_req.set_ts_ms(req.ts_ms());
    ack_req.set_max_inbox_seq(req.max_inbox_seq());
    ChatApiClient::getInstance()->Ack(ack_req);
}

// Note: All control operation handlers have been moved to ChatControlApiService (unary RPC)
// The following handlers are no longer in ChatSession:
// - HandleGetVerifyCode -> ChatControlApi.GetVerifyCode
// - HandleRegister -> ChatControlApi.Register  
// - HandleAuthLogin -> ChatControlApi.AuthLogin
// - HandleResetPassword -> ChatControlApi.ResetPassword
// - HandleGetUserProfile -> ChatControlApi.GetUserProfile
// - HandleUpdateUserProfile -> ChatControlApi.UpdateUserProfile
// - HandleSearchUser -> ChatControlApi.SearchUser
// - HandleGetContacts -> ChatControlApi.GetContacts
// - HandleGetContactRequests -> ChatControlApi.GetContactRequests
// - HandleSendContactReq -> ChatControlApi.SendContactRequest
// - HandleHandleContactReq -> ChatControlApi.HandleContactRequest
// - HandleSync -> ChatControlApi.Sync

// ==================== Inflight Management ====================

void ChatSession::TrackInflight(const im::Envelope& env) {
    if (finishing_.load()) return;

    InflightEntry entry;
    entry.env = env;
    entry.push_time = std::chrono::steady_clock::now();

    std::string msg_id = env.msg_id();  // 先保存msg_id，避免entry被move后使用

    {
        std::lock_guard<std::mutex> lock(mu_);
        inflight_[msg_id] = std::move(entry);
    }

    std::weak_ptr<ChatSession> weak_self = shared_from_this();
    TimerWheel::getInstance()->AddTask(5000, [weak_self, msg_id]() {
        if (auto self = weak_self.lock()) {
            std::lock_guard<std::mutex> lock(self->mu_);
            if (self->inflight_.erase(msg_id) > 0) {
                LOG_DEBUG("ChatSession: auto-cleanup inflight msg_id={}", msg_id);
            }
        }
    });

    LOG_DEBUG("ChatSession::TrackInflight: uid={}, msg_id={}, inflight_size={}",
              uid_.load(), msg_id, inflight_.size());
}

void ChatSession::AckInflight(const std::string& msg_id) {
    std::lock_guard<std::mutex> lock(mu_);

    auto it = inflight_.find(msg_id);
    if (it != inflight_.end()) {
        // Calculate latency for diagnostics
        auto now = std::chrono::steady_clock::now();
        auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.push_time).count();

        inflight_.erase(it);

        LOG_DEBUG("ChatSession::AckInflight: uid={}, msg_id={}, latency_ms={}, remaining={}",
                  uid_.load(), msg_id, latency_ms, inflight_.size());
    } else {
        // ACK for unknown message - might be delayed ACK or duplicate
        LOG_DEBUG("ChatSession::AckInflight: uid={}, msg_id={} not found (delayed or duplicate)",
                  uid_.load(), msg_id);
    }
}


void ChatSession::UpdateActiveTime() {
    last_active_time_ns.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_relaxed);
}

void ChatSession::StartHeartbeatCheck() {
    std::weak_ptr<ChatSession> weak_self = shared_from_this();

    TimerWheel::getInstance()->AddTask(30000, [weak_self]() {
        if (auto self = weak_self.lock()) {
            self->CheckHeartbeat();
        }
    });
}

void ChatSession::CheckHeartbeat() {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto last = last_active_time_ns.load(std::memory_order_relaxed);

    auto diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::duration(now - last)).count();

    if (diff_ms >= 30000) {
        // 超时，断开连接
        LOG_INFO("Session timeout, kicking offline.");
        this->KickAndFinish("Session timeout");
    } else {
        int next_check_delay = 30000 - diff_ms;
        std::weak_ptr<ChatSession> weak_self = shared_from_this();
        TimerWheel::getInstance()->AddTask(next_check_delay, [weak_self]() {
            if (auto self = weak_self.lock()) {
                self->CheckHeartbeat();
            }
        });
    }
}
