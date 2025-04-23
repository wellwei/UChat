/**
 * @file util.h
 * @brief Utility functions
 */

#pragma once

#include <functional>
#include <utility>
#include <sstream>
#include "proto_generated/user_service.pb.h"
#include "Logger.h"
#include "mysql/jdbc.h"

class Defer {
public:
    Defer(std::function<void()> func) : func_(std::move(func)) {}
    ~Defer() { func_(); }
private:
    std::function<void()> func_;
};

// 去除字符串两边的空格
inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, last - first + 1);
}

// 使用SHA-256哈希算法对密码进行哈希
inline std::string HashedPassword(const std::string& password) {
    std::string hashed_password = password;
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::hash<std::string>{}(password);
    return ss.str();
}

// 将mysql结果集转换为UserProfile
inline bool fromMysqlResult(const sql::ResultSet* rs, UserProfile& user_profile) {
    if (!rs) {
        return false;
    }
    try {
        user_profile.set_uid(rs->getUInt64("uid")); 
        if (!rs->isNull("username")) {
            user_profile.set_username(rs->getString("username"));
        } else {
            LOG_WARN("数据库中 uid={} 的 username 为 NULL", user_profile.uid());
        }
        if (!rs->isNull("nickname")) user_profile.set_nickname(rs->getString("nickname"));
        if (!rs->isNull("avatar_url")) user_profile.set_avatar_url(rs->getString("avatar_url"));
        if (!rs->isNull("email")) user_profile.set_email(rs->getString("email"));
        if (!rs->isNull("phone")) user_profile.set_phone(rs->getString("phone"));
        if (!rs->isNull("gender")) user_profile.set_gender(static_cast<UserGender>(rs->getInt("gender")));
        if (!rs->isNull("signature")) user_profile.set_signature(rs->getString("signature"));
        if (!rs->isNull("location")) user_profile.set_location(rs->getString("location"));
        if (!rs->isNull("status")) {
             user_profile.set_status(static_cast<UserStatus>(rs->getInt("status")));
        } else {
             user_profile.set_status(UserStatus::OFFLINE);
             LOG_WARN("数据库中 uid={} 的 status 为 NULL", user_profile.uid());
        }
        if (!rs->isNull("create_time")) {
             user_profile.set_create_time(rs->getString("create_time"));
        }
         if (!rs->isNull("update_time")) {
             user_profile.set_update_time(rs->getString("update_time"));
        }
        return true;
    } catch (sql::SQLException& e) {
        uint64_t failed_uid = 0;
        try {
            if (!rs->isNull("uid")) failed_uid = rs->getUInt64("uid");
        } catch (...) {}
        LOG_ERROR("Mysql 结果解析失败 (可能在处理 uid={}): {}", failed_uid, e.what());
        return false;
    } catch (std::exception& e) {
         LOG_ERROR("结果解析时发生标准异常: {}", e.what());
         return false;
    } catch (...) {
         LOG_ERROR("结果解析时发生未知异常");
         return false;
    }
}