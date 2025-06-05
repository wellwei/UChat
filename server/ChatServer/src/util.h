/**
 * @file util.h
 * @brief Utility functions
 */

#pragma once

#include <functional>
#include <utility>
#include <sstream>
#include "Logger.h"

class Defer {
public:
    explicit Defer(std::function<void()> func) : func_(std::move(func)) {}
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
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::hash<std::string>{}(password);
    return ss.str();
}