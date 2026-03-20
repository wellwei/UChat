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

    // M4: 群聊操作
    bool createGroup(const CreateGroupReq& request, GroupInfo& group);
    bool updateGroup(const UpdateGroupReq& request, GroupInfo& group);
    bool deleteGroup(uint64_t operator_uid, uint64_t group_id);
    bool getGroup(uint64_t requester_uid, uint64_t group_id, GroupInfo& group);
    bool searchGroups(const std::string& keyword, uint32_t limit, std::vector<GroupInfo>& groups);
    bool listMyGroups(uint64_t uid, std::vector<GroupInfo>& groups);
    bool joinGroup(uint64_t uid, uint64_t group_id);
    bool quitGroup(uint64_t uid, uint64_t group_id);
    bool getGroupMembers(uint64_t requester_uid, uint64_t group_id, std::vector<GroupMemberInfo>& members);
    
private:
    MysqlMgr();
    static bool fromMysqlResult(const sql::ResultSet* rs, UserProfile& user_profile);
    static bool fromGroupResult(const sql::ResultSet* rs, GroupInfo& group);
    std::shared_ptr<MysqlConnPool> conn_pool;
};
