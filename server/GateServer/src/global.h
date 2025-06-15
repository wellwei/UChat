//
// Created by wellwei on 2025/4/2.
//

#ifndef GATESERVER_GLOBAL_H
#define GATESERVER_GLOBAL_H

#include <nlohmann/json.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <iostream>
#include <chrono>
#include <utility>
#include <cassert>
#include <grpcpp/grpcpp.h>
#include <queue>
#include <sw/redis++/redis++.h>

enum ErrorCode {
    SUCCESS = 0,
    JSON_PARSE_ERROR = 1001,
    RPC_ERROR = 1002,
    INVALID_PARAMETER = 1003,
    TOO_MANY_REQUESTS = 1004,
    VERIFY_CODE_ERROR = 1005,
    USER_EXISTS = 1006,
    MYSQL_ERROR = 1008,
    EMAIL_NOT_REGISTERED = 1009,
    USERNAME_OR_PASSWORD_ERROR = 1010,
    NETWORK_ERROR = 1011,
    TOKEN_INVALID = 1012,
};

enum RequestType {
    GET = 1,
    POST = 2,
    PUT = 3,
};

// 获取当前主机名
inline std::string getHostName() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return {hostname};
    } else {
        return "Unknown";
    }
}

// 获取当前时间，格式为 YYYY-MM-DD_HH-MM-SS
inline std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &tm);
    return {buffer};
}

// 获取当前日期，格式为 YYYY-MM-DD
inline std::string getCurrentDate() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
    return {buffer};
}

// 获取当前时间点
inline std::chrono::system_clock::time_point getCurrentTimePoint() {
    return std::chrono::system_clock::now();
}


#endif //GATESERVER_GLOBAL_H
