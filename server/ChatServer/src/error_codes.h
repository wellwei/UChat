#pragma once

#include <cstdint>

namespace ErrorCodes {

    // SSS MM XXX
    // SSS: Server ID
    // MM: Module ID
    // XXX: Error Code

    constexpr int32_t kSuccess = 0;

    constexpr int32_t kGateServerId   = 100 * 100000; // 100 00 000
    constexpr int32_t kStatusServerId = 200 * 100000; // 200 00 000
    constexpr int32_t kChatServerId   = 300 * 100000; // 300 00 000
    constexpr int32_t kUserServerId   = 400 * 100000; // 400 00 000
    constexpr int32_t kVerifyCodeId   = 500 * 100000; // 500 00 000
    constexpr int32_t kCommonErrorId  = 900 * 100000; // 900 00 000

    // GateServer
    // TODO

    // StatusServer
    // TODO

    // ChatServer
    constexpr int32_t kChatAuthModule = kChatServerId + 1 * 1000; // 300 01 000
    constexpr int32_t kChatMsgModule = kChatServerId + 2 * 1000; // 300 02 000

    constexpr int32_t kErrChatTokenExpired = kChatAuthModule + 1; // 300 01 001: Token 已过期
    constexpr int32_t kErrChatTokenInvalid = kChatAuthModule + 2; // 300 01 002: Token 无效
    constexpr int32_t kErrChatTokenMissing = kChatAuthModule + 3; // 300 01 003: Token 缺失
    constexpr int32_t kErrChatTokenUserMismatch = kChatAuthModule + 4; // 300 01 004: Token 用户不匹配
    constexpr int32_t kErrChatTokenUidMissing = kChatAuthModule + 5; // 300 01 005: Token 用户 ID 缺失
    constexpr int32_t kErrChatTokenParseFailed = kChatAuthModule + 6; // 300 01 006: Token 解析失败
    constexpr int32_t kErrChatTokenVerifyFailed = kChatAuthModule + 7; // 300 01 007: Token 验证失败
    constexpr int32_t kErrChatUnauthenticated = kChatAuthModule + 8; // 300 01 008: 未认证
    constexpr int32_t kErrChatContentEmpty = kChatAuthModule + 9; // 300 01 009: 消息内容为空
    
    // 消息发送错误
    constexpr int32_t kErrChatSendFailed = kChatMsgModule + 1; // 300 02 001: 消息发送失败
    constexpr int32_t kErrChatUserNotFound = kChatMsgModule + 2; // 300 02 002: 用户不存在或不在线
    constexpr int32_t kErrChatServiceUnavailable = kChatMsgModule + 3; // 300 02 003: 服务暂时不可用

    // UserServer
    // TODO

    // VerifyCodeServer
    // TODO
    
    // Common
    constexpr int32_t kErrCommonInvalidJson = kCommonErrorId + 1; // 900 00 001: 无效的 JSON 格式
    constexpr int32_t kErrCommonInvalidType = kCommonErrorId + 2; // 900 00 002: 无效的消息类型
    constexpr int32_t kErrCommonInvalidField = kCommonErrorId + 3; // 900 00 003: 无效或缺失的字段
}

#include <string>
#include <unordered_map>

inline std::string getErrorString(int32_t error_code) {
    static const std::unordered_map<int32_t, std::string> error_strings = {
        {ErrorCodes::kSuccess, "Success"},
        {ErrorCodes::kErrChatTokenExpired, "Token expired"},
        {ErrorCodes::kErrChatTokenInvalid, "Invalid token"},
        {ErrorCodes::kErrChatTokenMissing, "Token missing"},
        {ErrorCodes::kErrChatTokenUserMismatch, "Token user mismatch"},
        {ErrorCodes::kErrChatTokenUidMissing, "Token uid missing"},
        {ErrorCodes::kErrChatTokenParseFailed, "Token parse failed"},
        {ErrorCodes::kErrChatTokenVerifyFailed, "Token verify failed"},
        {ErrorCodes::kErrCommonInvalidJson, "Invalid JSON format"},
        {ErrorCodes::kErrCommonInvalidType, "Invalid message type"},
        {ErrorCodes::kErrCommonInvalidField, "Invalid or missing field"},
        {ErrorCodes::kErrChatUnauthenticated, "Unauthenticated"},
        {ErrorCodes::kErrChatContentEmpty, "Content cannot be empty"},
        {ErrorCodes::kErrChatSendFailed, "Message send failed"},
        {ErrorCodes::kErrChatUserNotFound, "User not found or offline"},
        {ErrorCodes::kErrChatServiceUnavailable, "Service temporarily unavailable"},
    };

    auto it = error_strings.find(error_code);
    if (it != error_strings.end()) {
        return it->second;
    }

    return "Unknown error code: " + std::to_string(error_code);
}