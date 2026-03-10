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

static bool IsVerifyEnabled() {
    std::string val = (*ConfigMgr::getInstance())["VerifyServer"].get("enabled", "true");
    return val != "false" && val != "0";
}

ChatSession::ChatSession(SessionRegistry* registry, const std::string& gateway_id, int online_ttl_seconds)
    : registry_(registry), gateway_id_(gateway_id), online_ttl_seconds_(online_ttl_seconds) {
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
        case im::ClientFrame::kPull:
            HandlePull(in_frame_.pull());
            break;
        case im::ClientFrame::kGetVerifyCode:
            HandleGetVerifyCode(in_frame_.get_verify_code());
            break;
        case im::ClientFrame::kRegisterReq:
            HandleRegister(in_frame_.register_req());
            break;
        case im::ClientFrame::kAuthLogin:
            HandleAuthLogin(in_frame_.auth_login());
            break;
        case im::ClientFrame::kResetPassword:
            HandleResetPassword(in_frame_.reset_password());
            break;
        default:
            LOG_WARN("ChatSession: unknown frame type from uid={}", uid_.load());
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

    // Write presence to Redis
    std::string key = "online:" + std::to_string(uid_);
    std::string value = gateway_id_ + ":" + std::to_string(session_ver_);
    RedisMgr::getInstance()->set(key, value, online_ttl_seconds_);

    // Register in session registry
    registry_->Bind(uid_, shared_from_this());

    LOG_INFO("ChatSession: uid={} logged in, session_ver={}", uid_.load(), session_ver_.load());

    im::ServerFrame resp;
    auto* lr = resp.mutable_login();
    lr->set_ok(true);
    lr->set_session_ver(session_ver_);
    Enqueue(std::move(resp));
}

void ChatSession::HandleHeartbeat(const im::Heartbeat& req) {
    if (authed_) {
        std::string key = "online:" + std::to_string(uid_);
        RedisMgr::getInstance()->expire(key, online_ttl_seconds_);
    }

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
    // Forward ACK to ChatServer (best-effort, fire and forget for M4)
    im::AckUpReq ack_req;
    ack_req.set_uid(uid_);
    ack_req.set_msg_id(req.msg_id());
    ack_req.set_type(req.type());
    ack_req.set_ts_ms(req.ts_ms());
    ChatApiClient::getInstance()->Ack(ack_req);
}

void ChatSession::HandlePull(const im::PullHistoryReq& req) {
    if (!authed_) return;
    // M5: will implement full history. Return empty for now.
    im::ServerFrame frame;
    frame.mutable_history(); // empty PullHistoryResp
    Enqueue(std::move(frame));
}

void ChatSession::HandleGetVerifyCode(const im::GetVerifyCodeReq& req) {
    const std::string& email = req.email();
    if (email.empty()) {
        im::ServerFrame frame;
        auto* r = frame.mutable_get_verify_code();
        r->set_ok(false);
        r->set_message("Email is required");
        Enqueue(std::move(frame));
        return;
    }

    if (!IsVerifyEnabled()) {
        im::ServerFrame frame;
        auto* r = frame.mutable_get_verify_code();
        r->set_ok(true);
        r->set_email(email);
        r->set_message("Verification service disabled");
        Enqueue(std::move(frame));
        return;
    }

    auto redis = RedisMgr::getInstance();
    std::string throttle_key = "throttle_verify_" + email;
    if (redis->exists(throttle_key)) {
        im::ServerFrame frame;
        auto* r = frame.mutable_get_verify_code();
        r->set_ok(false);
        r->set_message("Too many requests. Please try again later.");
        Enqueue(std::move(frame));
        return;
    }

    grpc::Status status = VerifyGrpcClient::getInstance()->getVerifyCode(email);
    im::ServerFrame frame;
    auto* r = frame.mutable_get_verify_code();
    if (!status.ok()) {
        r->set_ok(false);
        r->set_message(status.error_message());
    } else {
        redis->set(throttle_key, "1", 60);
        r->set_ok(true);
        r->set_email(email);
        r->set_message("Verification code sent");
    }
    Enqueue(std::move(frame));
}

void ChatSession::HandleRegister(const im::RegisterReq& req) {
    bool verify_on = IsVerifyEnabled();
    if (req.email().empty() || req.username().empty() || req.password().empty() ||
        (verify_on && req.verify_code().empty())) {
        im::ServerFrame frame;
        auto* r = frame.mutable_register_resp();
        r->set_ok(false);
        r->set_message("Missing required fields");
        Enqueue(std::move(frame));
        return;
    }

    if (verify_on) {
        auto redis = RedisMgr::getInstance();
        std::string code_key = "verify_code_" + req.email();
        if (!redis->exists(code_key) || redis->get(code_key) != req.verify_code()) {
            im::ServerFrame frame;
            auto* r = frame.mutable_register_resp();
            r->set_ok(false);
            r->set_message("Invalid verification code");
            Enqueue(std::move(frame));
            return;
        }
    }

    uint64_t uid = 0;
    uint32_t code = UserServiceClient::getInstance()->CreateUser(
        req.username(), req.password(), req.email(), uid);

    im::ServerFrame frame;
    auto* r = frame.mutable_register_resp();
    if (code == ErrorCode::SUCCESS) {
        if (verify_on) {
            RedisMgr::getInstance()->del("verify_code_" + req.email());
        }
        r->set_ok(true);
        r->set_uid(static_cast<int64_t>(uid));
        r->set_message("Registration successful");
    } else if (code == ErrorCode::USER_EXISTS) {
        r->set_ok(false);
        r->set_message("User already exists");
    } else {
        r->set_ok(false);
        r->set_message("Registration failed");
    }
    Enqueue(std::move(frame));
}

void ChatSession::HandleAuthLogin(const im::AuthLoginReq& req) {
    if (req.handle().empty() || req.password().empty()) {
        im::ServerFrame frame;
        auto* r = frame.mutable_auth_login();
        r->set_ok(false);
        r->set_message("Handle and password are required");
        Enqueue(std::move(frame));
        return;
    }

    nlohmann::json resp_json;
    bool ok = UserServiceClient::getInstance()->VerifyCredentials(
        req.handle(), req.password(), resp_json);

    im::ServerFrame frame;
    auto* r = frame.mutable_auth_login();
    if (ok) {
        r->set_ok(true);
        r->set_uid(resp_json["uid"].get<int64_t>());
        r->set_token(resp_json["token"].get<std::string>());
        r->set_message("Login successful");
    } else {
        r->set_ok(false);
        r->set_message(resp_json.value("message", "Invalid credentials"));
    }
    Enqueue(std::move(frame));
}

void ChatSession::HandleResetPassword(const im::ResetPasswordReq& req) {
    bool verify_on = IsVerifyEnabled();
    if (req.email().empty() || req.new_password().empty() ||
        (verify_on && req.verify_code().empty())) {
        im::ServerFrame frame;
        auto* r = frame.mutable_reset_password();
        r->set_ok(false);
        r->set_message("Missing required fields");
        Enqueue(std::move(frame));
        return;
    }

    if (verify_on) {
        auto redis = RedisMgr::getInstance();
        std::string code_key = "verify_code_" + req.email();
        if (!redis->exists(code_key) || redis->get(code_key) != req.verify_code()) {
            im::ServerFrame frame;
            auto* r = frame.mutable_reset_password();
            r->set_ok(false);
            r->set_message("Invalid verification code");
            Enqueue(std::move(frame));
            return;
        }
    }

    im::ServerFrame frame;
    auto* r = frame.mutable_reset_password();
    if (UserServiceClient::getInstance()->ResetPassword(req.email(), req.new_password())) {
        if (verify_on) {
            RedisMgr::getInstance()->del("verify_code_" + req.email());
        }
        r->set_ok(true);
        r->set_message("Password reset successful");
    } else {
        r->set_ok(false);
        r->set_message("Failed to reset password");
    }
    Enqueue(std::move(frame));
}
