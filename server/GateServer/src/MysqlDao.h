//
// Created by wellwei on 2025/4/10.
//

#ifndef GATESERVER_MYSQLDAO_H
#define GATESERVER_MYSQLDAO_H

#include <memory>
#include "MysqlConnPool.h"
#include "UserInfo.h"

class MysqlDao {
    friend class MysqlMgr;

public:
    ~MysqlDao();

    MysqlDao();

private:

    int registerUser(const std::string &name, const std::string &email, const std::string &password);

    int getUidByEmail(const std::string &email);

    int getUidByUsername(const std::string &username);

    UserInfo getUserInfo(int uid);

    bool resetPassword(int uid, const std::string &password);

    bool checkPassword(int uid, const std::string &password);

    std::unique_ptr<MysqlConnPool> _conn_pool; // MySQL 连接池
};


#endif //GATESERVER_MYSQLDAO_H
