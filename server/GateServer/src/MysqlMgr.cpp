//
// Created by wellwei on 2025/4/10.
//

#include "MysqlMgr.h"
#include "util.h"

MysqlMgr::MysqlMgr() {
    _dao = std::make_unique<MysqlDao>();
}

MysqlMgr::~MysqlMgr() {
    _dao.reset();
}

int MysqlMgr::registerUser(const std::string &name, const std::string &email, const std::string &password) {
    return _dao->RegisterUser(name, email, hashPassword(password));
}