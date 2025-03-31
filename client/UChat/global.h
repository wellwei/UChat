//
// Created by wellwei on 2025/3/28.
//

#ifndef UCHAT_GLOBAL_H
#define UCHAT_GLOBAL_H

#include <QWidget>
#include <functional>
#include <QStyle>

extern std::function<void(QWidget * )> repolish;

// 服务器 URL
const QString SERVER_URL = "http://localhost:8080";

// HTTP 请求类型 ID
enum ReqId {
    ID_GET_CAPTCHA = 1001,
    ID_REGISTER = 1002,
    ID_LOGIN = 1003,
};

// 错误码
enum ErrorCodes {
    SUCCESS = 0,
    ERR_JSON = 1, //Json 解析失败
    ERR_NETWORK = 2, //网络错误
};

// 模块类型
enum Modules {
    REGISTERMOD = 0,
    LOGINMOD = 1,
};

#endif //UCHAT_GLOBAL_H
