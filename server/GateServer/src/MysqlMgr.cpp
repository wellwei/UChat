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
    return _dao->registerUser(name, email, hashPassword(password));
}

int MysqlMgr::getUidByEmail(const std::string &email) {
    if (!isValidEmail(email)) {
        return -1; // Invalid email format
    }
    return _dao->getUidByEmail(email);
}

bool MysqlMgr::resetPassword(int uid, const std::string &password) {
    if (uid <= 0 || password.empty()) {
        return false; // Invalid parameters
    }
    return _dao->resetPassword(uid, hashPassword(password));
}

int MysqlMgr::getUidByUsername(const std::string &username) {
    if (!isValidUsername(username)) {
        return -1; // Invalid username format
    }
    return _dao->getUidByUsername(username);
}

bool MysqlMgr::checkPassword(int uid, const std::string &password) {
    if (uid <= 0 || password.empty()) {
        return false; // Invalid parameters
    }
    return _dao->checkPassword(uid, hashPassword(password));
}

UserInfo MysqlMgr::getUserInfo(int uid) {
    if (uid <= 0) {
        return {}; // Invalid uid
    }
    return _dao->getUserInfo(uid);
}
