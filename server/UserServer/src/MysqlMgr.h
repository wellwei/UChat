#pragma once

#include "MysqlConnPool.h"
#include "Singleton.h"
#include "userserver.pb.h"
#include <mysql/jdbc.h>

class MysqlMgr : public Singleton<MysqlMgr> {
    friend class Singleton<MysqlMgr>;

public:
    ~MysqlMgr() = default;

    bool exists_user(const std::string& username, const std::string& email);

    bool verifyCredentials(const std::string& handle, const std::string& password, uint64_t &uid);
    bool createUser(const std::string &username, const std::string &password, const std::string &email, uint64_t &uid);
    bool getUserProfile(uint64_t uid, UserProfile& user_profile);
    bool updateUserProfile(uint64_t uid, const UserProfile& user_profile);

    bool resetPassword(const std::string& email, const std::string& new_password);

    bool searchUser(const std::string& keyword, std::vector<UserProfile>& users);

    // M3: 联系人操作
    bool sendContactRequest(uint64_t from_uid, uint64_t to_uid, const std::string& note, uint64_t& out_request_id);
    bool handleContactRequest(uint64_t request_id, uint64_t handler_uid, bool accept, uint64_t& out_from_uid);
    bool getContacts(uint64_t uid, std::vector<ContactEntry>& contacts);
    bool getContactRequests(uint64_t uid, std::vector<ContactRequest>& requests);
    
private:
    MysqlMgr();
    static bool fromMysqlResult(const sql::ResultSet* rs, UserProfile& user_profile);
    std::shared_ptr<MysqlConnPool> conn_pool;
};