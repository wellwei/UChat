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

    bool getContacts(uint64_t uid, std::vector<ContactInfo>& contacts);
    
    bool searchUser(const std::string& keyword, std::vector<UserProfile>& users);
    
    // 联系人请求相关的方法
    bool sendContactRequest(uint64_t requester_id, uint64_t addressee_id, const std::string& message, uint64_t& request_id);
    bool handleContactRequest(uint64_t request_id, uint64_t user_id, ContactRequestStatus action);
    bool getContactRequests(uint64_t user_id, ContactRequestStatus status, std::vector<ContactRequestInfo>& requests);
    bool getContactRequestById(uint64_t request_id, ContactRequestInfo& request_info);
    bool areContacts(uint64_t user_id_a, uint64_t user_id_b);
    bool hasPendingRequest(uint64_t requester_id, uint64_t addressee_id);
    
private:
    MysqlMgr();
    static bool fromMysqlResult(const sql::ResultSet* rs, UserProfile& user_profile);
    static bool fromMysqlResult(const sql::ResultSet* rs, ContactRequestInfo& request_info);
    std::shared_ptr<MysqlConnPool> conn_pool;
};