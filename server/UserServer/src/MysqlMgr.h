#pragma once

#include "MysqlConnPool.h"
#include "Singleton.h"
#include "proto_generated/user_service.pb.h"
#include <mysql/jdbc.h>

class MysqlMgr : public Singleton<MysqlMgr> {
    friend class Singleton<MysqlMgr>;

public:
    ~MysqlMgr() = default;

    bool findById(uint64_t uid, UserProfile& user_profile);
    bool findByUsername(const std::string& username, UserProfile& user_profile);

    bool updateById(uint64_t uid, const UserProfile& user_profile);
    bool updateByUsername(const std::string& username, const UserProfile& user_profile);

    bool deleteById(uint64_t uid);
    bool deleteByUsername(const std::string& username);

    bool verifyCredentials(const std::string& handle, const std::string& password, uint64_t &uid, UserProfile& user_profile);
    bool createUser(const std::string &username, const std::string &password, const std::string &email, uint64_t &uid);
    bool getUserProfile(uint64_t uid, UserProfile& user_profile);
    bool updateUserProfile(uint64_t uid, const UserProfile& user_profile);

    bool resetPassword(const std::string& email, const std::string& new_password);  
    
private:
    MysqlMgr();
    static bool fromMysqlResult(const sql::ResultSet* rs, UserProfile& user_profile);
    std::shared_ptr<MysqlConnPool> conn_pool;
};