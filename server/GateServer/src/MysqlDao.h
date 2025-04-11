//
// Created by wellwei on 2025/4/10.
//

#ifndef GATESERVER_MYSQLDAO_H
#define GATESERVER_MYSQLDAO_H

#include <memory>
#include "MysqlConnPool.h"

class MysqlDao {
public:
    MysqlDao();

    ~MysqlDao();

    int RegisterUser(const std::string &name, const std::string &email, const std::string &password);

private:
    std::unique_ptr<MysqlConnPool> _conn_pool; // MySQL 连接池
};


#endif //GATESERVER_MYSQLDAO_H
