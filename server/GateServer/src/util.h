//
// Created by wellwei on 2025/4/1.
//

#ifndef GATESERVER_UTIL_H
#define GATESERVER_UTIL_H

#include "global.h"
#include <regex>
#include <random>
#include <openssl/sha.h>

inline unsigned char decToHex(unsigned char dec) {
    return dec > 9 ? dec + 55 : dec + 48; // 10-15 转换为 A-F，0-9 转换为 0-9
}

inline unsigned char hexToDec(unsigned char hex) {
    if (hex >= '0' && hex <= '9') {
        return hex - '0';
    } else if (hex >= 'A' && hex <= 'F') {
        return hex - 'A' + 10;
    } else if (hex >= 'a' && hex <= 'f') {
        return hex - 'a' + 10;
    } else {
        assert(false && "Invalid hex character");
    }
}

/**
 * @brief URL编码
 * @param str 待编码的字符串
 * @return 编码后的字符串
 */
inline std::string urlEncode(const std::string &str) {
    std::string encoded;
    for (char c: str) {
        // 如果字符是字母、数字或某些特殊字符，则直接添加到结果中
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else if (c == ' ') {
            // 空格转换为加号
            encoded += '+';
        } else {
            // 其他字符转换为%XX 格式
            encoded += '%';
            encoded += decToHex(static_cast<unsigned char>(c) >> 4);    // 高四位
            encoded += decToHex(static_cast<unsigned char>(c) & 0x0F);  // 低四位
        }
    }
    return encoded;
}

/**
 * @brief URL解码
 * @param str 待解码的字符串
 * @return 解码后的字符串
 */
inline std::string urlDecode(const std::string &str) {
    std::string decoded;
    size_t len = str.length();
    for (size_t i = 0; i < len; ++i) {
        char c = str[i];
        if (c == '%') {
            // 如果遇到%，则读取后两位作为十六进制数
            if (i + 2 < len) {
                char hex1 = str[i + 1];
                char hex2 = str[i + 2];
                decoded += static_cast<char>(hexToDec(hex1) * 16 + hexToDec(hex2));
                i += 2; // 跳过已处理的两个字符
            } else {
                assert(false && "Invalid URL encoding");
            }
        } else if (c == '+') {
            // 将加号转换为空格
            decoded += ' ';
        } else {
            decoded += c; // 其他字符直接添加
        }
    }
    return decoded;
}

// validate email format
inline bool isValidEmail(const std::string &email) {
    const std::regex pattern(R"((\w+)(\.\w+)*@(\w+)(\.\w+)+)");
    return std::regex_match(email, pattern);
}

// validate username format
inline bool isValidUsername(const std::string &username) {
    // 6-16 位 大小写字母、数字、下划线，不能以数字开头
    const std::regex pattern(R"((?!^\d)[a-zA-Z0-9_]{6,16})");
    return std::regex_match(username, pattern);
}

// validate password format
inline bool isValidPassword(const std::string &password) {
    // 8-20 位 字母、数字或特殊字符（如 !@#$%^&*()_+-.）
    const std::regex pattern(R"((?=.*[a-zA-Z])(?=.*\d)[a-zA-Z\d!@#$%^&*()_+\-=]{8,20})");
    return std::regex_match(password, pattern);
}

// 对密码进行 SHA256 哈希
inline std::string hashPassword(const std::string &password) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(password.c_str()), password.length(), hash);
    std::string hashedPassword;
    // 将每个字节转换为十六进制字符串
    for (unsigned char i: hash) {
        hashedPassword += decToHex(i >> 4);
        hashedPassword += decToHex(i & 0x0F);
    }
    return hashedPassword;
}

// 自动释放资源的类
class Defer {
public:
    explicit Defer(std::function<void()> func) : func_(std::move(func)) {}

    ~Defer() {
        if (func_) {
            func_();
        }
    }

private:
    std::function<void()> func_;
};

#endif //GATESERVER_UTIL_H
