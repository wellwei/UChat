//
// Created by wellwei on 2025/3/28.
//

#ifndef UCHAT_GLOBAL_H
#define UCHAT_GLOBAL_H

#include <QWidget>

extern std::function<void(QWidget *)> repolish;

// 服务器 URL
extern QString GATE_SERVER_URL;

// HTTP 请求类型 ID
enum ReqId {
    ID_GET_VERIFYCODE = 1001,
    ID_REGISTER = 1002,
    ID_LOGIN = 1003,
    ID_FORGET_PWD_REQUEST_CODE = 1004,
    ID_RESET_PASSWORD = 1005,
    ID_GET_CHAT_SERVER = 1006,
    ID_GET_USER_PROFILE = 1007,
    ID_ADD_CONTACT = 1008,
    ID_GET_CONTACTS = 1009,
    ID_SEARCH_USER = 1010,
    ID_SEND_CONTACT_REQUEST = 1011,
    ID_HANDLE_CONTACT_REQUEST = 1012,
    ID_GET_CONTACT_REQUESTS = 1013,
};

// 错误码
enum ErrorCodes {
    SUCCESS = 0,
    JSON_PARSE_ERROR = 1001,
    RPC_ERROR = 1002,
    INVALID_PARAMETER = 1003,
    TOO_MANY_REQUESTS = 1004,
    VERIFY_CODE_ERROR = 1005,
    USER_EXISTS = 1006,
    MYSQL_ERROR = 1008,
    EMAIL_NOT_REGISTERED = 1009,
    USERNAME_NOT_REGISTERED = 1010,
    NETWORK_ERROR = 1011,
};

// 错误码映射
inline const char *getErrorMessage(ErrorCodes code) {
    switch (code) {
        case SUCCESS:
            return "Success";
        case JSON_PARSE_ERROR:
            return "JSON parse error";
        case RPC_ERROR:
            return "RPC error";
        case INVALID_PARAMETER:
            return "Invalid parameter";
        case TOO_MANY_REQUESTS:
            return "Too many requests";
        case VERIFY_CODE_ERROR:
            return "Verify code error";
        case USER_EXISTS:
            return "Username or Email already exists";
        case MYSQL_ERROR:
            return "MySQL error";
        case EMAIL_NOT_REGISTERED:
            return "Email not registered";
        case USERNAME_NOT_REGISTERED:
            return "Username or password error";
        case NETWORK_ERROR:
            return "Network error";
        default:
            return "Unknown error";
    }
}

// 模块类型
enum Modules {
    REGISTERMOD = 0,
    LOGINMOD = 1,
    RESETMOD = 2,
    CHATMOD = 3
};

// 联系人请求状态
enum ContactRequestStatus {
    PENDING = 0,
    ACCEPTED = 1,
    REJECTED = 2
};

struct UserProfile {
    quint64 uid;
    QString username;
    QString nickname;
    QString avatar_url;
    QString email;
    QString phone;
    QString gender;
    QString signature;
    QString location;
    QString status;
    QString create_time;
    QString update_time;
};

struct ContactRequest {
    quint64 request_id;
    quint64 requester_id;
    QString requester_name;
    QString requester_nickname;
    QString requester_avatar;
    QString request_message;
    ContactRequestStatus status;
    QString created_at;
    QString updated_at;
};

struct ClientInfo {
    QString host;
    QString port;
    QString token;
    quint64 uid;
    UserProfile userProfile;
};

#endif //UCHAT_GLOBAL_H
