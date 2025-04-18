//
// Created by wellwei on 25-4-17.
//

#ifndef GATESERVER_USERINFO_H
#define GATESERVER_USERINFO_H

#include <string>

// 用户信息序列化类
class UserInfo {
public:
    UserInfo();

    uint64_t uid{};
    std::string username;
    std::string email;
    std::string password;
    int status{};
    uint64_t last_login_time{};
    uint64_t create_time{};
};


#endif //GATESERVER_USERINFO_H
