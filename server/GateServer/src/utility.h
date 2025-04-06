//
// Created by wellwei on 2025/4/1.
//

#ifndef GATESERVER_UTILITY_H
#define GATESERVER_UTILITY_H

#include "global.h"

unsigned char decToHex(unsigned char dec) {
    return dec > 9 ? dec + 55 : dec + 48; // 10-15 转换为 A-F，0-9 转换为 0-9
}

unsigned char hexToDec(unsigned char hex) {
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
std::string urlEncode(const std::string &str) {
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
std::string urlDecode(const std::string &str) {
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

#endif //GATESERVER_UTILITY_H
