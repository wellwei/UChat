//
// Created by wellwei on 2025/4/10.
//

#ifndef GATESERVER_MYSQLMGR_H
#define GATESERVER_MYSQLMGR_H

#include "Singleton.h"
#include "MysqlDao.h"

class MysqlMgr : public Singleton<MysqlMgr> {
    friend class Singleton<MysqlMgr>;

public:
    ~MysqlMgr();

    int registerUser(const std::string &name, const std::string &email, const std::string &password);

private:
    MysqlMgr();

    std::unique_ptr<MysqlDao> _dao;
};


#endif //GATESERVER_MYSQLMGR_H
