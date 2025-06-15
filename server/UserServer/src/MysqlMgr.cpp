#include "MysqlMgr.h"
#include "util.h"
#include "ConfigMgr.h"  
#include "userserver.pb.h"
#include "Logger.h"
#include "RedisMgr.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

inline std::string getUserProfileCacheKey(uint64_t uid) {
    return "user_profile:" + std::to_string(uid);
}   

MysqlMgr::MysqlMgr() {
    auto &config = *ConfigMgr::getInstance();
    std::string server_addr = config["Mysql"]["host"] + ":" + config["Mysql"]["port"];
    LOG_DEBUG("MysqlMgr::MysqlMgr() server_addr: {}", server_addr);
    conn_pool = std::make_shared<MysqlConnPool>(server_addr, config["Mysql"]["user"], config["Mysql"]["password"], config["Mysql"]["database"]);
}

bool MysqlMgr::exists_user(const std::string& username, const std::string& email) {
    auto conn = conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("MysqlMgr::exists_user() 获取数据库连接失败");
        return false;
    }
    Defer defer([this, &conn]() {
        conn_pool->returnConnection(std::move(conn));
    });

    try {
        std::string query = "SELECT COUNT(*) FROM users WHERE username = ? OR email = ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, username);
        stmt->setString(2, email);
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
        if (rs->next()) {
            return rs->getInt(1) > 0;
        }
        return false;
    } catch (sql::SQLException& e) {
        LOG_ERROR("MysqlMgr::exists_user() 查询失败: {}", e.what());
        return false;
    } catch (std::exception& e) {
        LOG_ERROR("MysqlMgr::exists_user() 查询失败: {}", e.what());
        return false;
    } catch (...) {
        LOG_ERROR("MysqlMgr::exists_user() 查询失败: Unknown error");
        return false;
    }
}

bool MysqlMgr::verifyCredentials(const std::string& handle, const std::string& password, uint64_t &uid) {
    auto conn = conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("MysqlMgr::verifyCredentials() 获取数据库连接失败");
        return false;
    }
    Defer defer([this, &conn]() {
        conn_pool->returnConnection(std::move(conn));
    });

    try {
        std::string hashed_password = HashedPassword(password);
        std::string query = "SELECT uid FROM users WHERE (username = ? OR email = ?) AND password = ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, handle);
        stmt->setString(2, handle);
        stmt->setString(3, hashed_password);
        
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
        if (rs->next()) {
            uid = rs->getUInt64(1);
            return true;
        }
        return false;
    } catch (sql::SQLException& e) {
        LOG_ERROR("Mysql 查询失败: {}", e.what());
        return false;
    } catch (std::exception& e) {
        LOG_ERROR("Mysql <UNK>: {}", e.what());
        return false;
    } catch (...) {
        LOG_ERROR("Mysql <UNK>: Unknown error");
        return false;
    }
}

bool MysqlMgr::createUser(const std::string &username, const std::string &password, const std::string &email, uint64_t &uid) {
    auto conn = conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("MysqlMgr::createUser() 获取数据库连接失败");
        return false;
    }
    Defer defer([this, &conn]() {
        conn_pool->returnConnection(std::move(conn));
    });
    try {
        std::string hashed_password = HashedPassword(password);
        std::string query = "INSERT INTO users (username, password, email, create_time, update_time) VALUES (?, ?, ?, NOW(), NOW())";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, username);
        stmt->setString(2, hashed_password);
        stmt->setString(3, email);
        stmt->executeUpdate();
        
        // 获取自动生成的uid
        std::unique_ptr<sql::Statement> idStmt(conn->createStatement());
        std::unique_ptr<sql::ResultSet> rs(idStmt->executeQuery("SELECT LAST_INSERT_ID()"));
        if (rs->next()) {
            uid = rs->getUInt64(1);
            LOG_DEBUG("MysqlMgr::createUser() uid: {}", uid);
            return true;
        } else {
            LOG_ERROR("MysqlMgr::createUser() 获取自增ID失败");
        }
    
        return false;
    } catch (sql::SQLException& e) {
        LOG_ERROR("MysqlMgr::createUser() 插入失败: {}", e.what());
        return false;
    }
}

bool MysqlMgr::getUserProfile(uint64_t uid, UserProfile& user_profile) {
    auto redis_mgr = RedisMgr::getInstance();  
    auto cache_key = getUserProfileCacheKey(uid);

    if (const auto cache_value = redis_mgr->get(cache_key)) {
        LOG_DEBUG("缓存命中, 从缓存中获取用户信息");
        if (user_profile.ParseFromString(cache_value.value())) {
            LOG_DEBUG("MysqlMgr::getUserProfile() 从缓存中获取用户信息成功");
            return true;
        } else {
            LOG_ERROR("MysqlMgr::getUserProfile() 解析缓存失败");
            redis_mgr->del(cache_key);
            return false;
        }
    }

    // 缓存未命中, 从数据库中获取用户信息
    auto conn = conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("MysqlMgr::getUserProfile() 获取数据库连接失败");
        return false;
    }
    Defer defer([this, &conn]() {
        conn_pool->returnConnection(std::move(conn));
    });
    try {
        std::string query = "SELECT uid, username, nickname, avatar_url, email, phone, gender, signature, location, status, create_time, update_time FROM users WHERE uid = ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setUInt64(1, uid);
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
        if (rs->next()) {
            if (fromMysqlResult(rs.get(), user_profile)) {
                std::string serialized_user_profile;
                if (user_profile.SerializeToString(&serialized_user_profile)) {
                    redis_mgr->set(cache_key, serialized_user_profile);
                    LOG_DEBUG("MysqlMgr::getUserProfile() 将用户信息写入缓存");
                } else {
                    LOG_ERROR("MysqlMgr::getUserProfile() 序列化用户信息失败");
                }
                return true;
            }
        }
        return false;
    } catch (sql::SQLException& e) {
        LOG_ERROR("MysqlMgr::getUserProfile() 查询失败: {}", e.what()); 
        return false;   
    }
}

bool MysqlMgr::updateUserProfile(uint64_t uid, const UserProfile& user_profile) {
    auto conn = conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("MysqlMgr::updateUserProfile() 获取数据库连接失败");
        return false;
    }
    Defer defer([this, &conn]() {
        conn_pool->returnConnection(std::move(conn));
    });
    try {
        std::string query = "UPDATE users SET username = ?, nickname = ?, avatar_url = ?, email = ?, phone = ?, gender = ?, signature = ?, location = ?, status = ?, update_time = NOW() WHERE uid = ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, user_profile.username());
        stmt->setString(2, user_profile.nickname());
        stmt->setString(3, user_profile.avatar_url());
        stmt->setString(4, user_profile.email());
        stmt->setString(5, user_profile.phone());
        stmt->setInt(6, user_profile.gender());
        stmt->setString(7, user_profile.signature());
        stmt->setString(8, user_profile.location());
        stmt->setInt(9, user_profile.status());
        stmt->setUInt64(10, uid);
        int affected_rows = stmt->executeUpdate();
        if (affected_rows > 0) {
            // 更新成功，数据库有变化，删除缓存
            auto redis_mgr = RedisMgr::getInstance();
            auto cache_key = getUserProfileCacheKey(uid);
            if (redis_mgr->del(cache_key)) {
                LOG_DEBUG("MysqlMgr::updateUserProfile() 更新成功，删除缓存");
            } else {
                LOG_ERROR("MysqlMgr::updateUserProfile() 删除缓存失败 for user {}", uid);
            }
            
            // 更新缓存
            std::string serialized_user_profile;
            if (user_profile.SerializeToString(&serialized_user_profile)) {
                redis_mgr->set(cache_key, serialized_user_profile);
                LOG_DEBUG("MysqlMgr::updateUserProfile() 更新缓存成功");
            } else {
                LOG_ERROR("MysqlMgr::updateUserProfile() 序列化用户信息失败 for user {}", uid);
            }
            
            // 发布用户信息更新事件
            nlohmann::json pub_message;
            pub_message["uid"] = uid;
            pub_message["event"] = "user_profile_update";
            // 可包含更新的字段 pub_message["updated_fields"] = user_profile.GetFieldMask();

            if (redis_mgr->publish("", pub_message.dump())) {
                LOG_DEBUG("MysqlMgr::updateUserProfile() 发布用户信息更新事件成功");
            } else {
                LOG_ERROR("MysqlMgr::updateUserProfile() 发布用户信息更新事件失败");
            }
        } else {
            LOG_WARN("MysqlMgr::updateUserProfile() 更新失败，数据库没有变化 for user {}", uid);
        }
        return true;
    } catch (sql::SQLException& e) {
        LOG_ERROR("MysqlMgr::updateUserProfile() 更新失败: {}", e.what());
        return false;
    }
}

// 重置密码 
bool MysqlMgr::resetPassword(const std::string& email, const std::string& new_password) {
    auto conn = conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("MysqlMgr::resetPassword() 获取数据库连接失败");
        return false;
    }
    Defer defer([this, &conn]() {
        conn_pool->returnConnection(std::move(conn));
    });
    try {
        std::string hashed_password = HashedPassword(new_password);
        std::string query = "UPDATE users SET password = ? WHERE email = ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, hashed_password);
        stmt->setString(2, email);
        stmt->executeUpdate();
        return true;
    } catch (sql::SQLException& e) {
        LOG_ERROR("MysqlMgr::resetPassword() 更新失败: {}", e.what());
        return false;
    }
}           

// 将mysql结果集转换为UserProfile
bool MysqlMgr::fromMysqlResult(const sql::ResultSet* rs, UserProfile& user_profile) {
    if (!rs) {
        return false;
    }
    try {
        user_profile.set_uid(rs->getUInt64("uid"));
        if (!rs->isNull("username")) {
            user_profile.set_username(rs->getString("username"));
        } else {
            LOG_WARN("数据库中 uid={} 的 username 为 NULL", user_profile.uid());
        }
        if (!rs->isNull("nickname")) user_profile.set_nickname(rs->getString("nickname"));
        if (!rs->isNull("avatar_url")) user_profile.set_avatar_url(rs->getString("avatar_url"));
        if (!rs->isNull("email")) user_profile.set_email(rs->getString("email"));
        if (!rs->isNull("phone")) user_profile.set_phone(rs->getString("phone"));
        if (!rs->isNull("gender")) user_profile.set_gender(static_cast<UserGender>(rs->getInt("gender")));
        if (!rs->isNull("signature")) user_profile.set_signature(rs->getString("signature"));
        if (!rs->isNull("location")) user_profile.set_location(rs->getString("location"));
        if (!rs->isNull("status")) {
             user_profile.set_status(static_cast<UserStatus>(rs->getInt("status")));
        } else {
             user_profile.set_status(UserStatus::OFFLINE);
             LOG_WARN("数据库中 uid={} 的 status 为 NULL", user_profile.uid());
        }
        if (!rs->isNull("create_time")) {
             user_profile.set_create_time(rs->getString("create_time"));
        }
         if (!rs->isNull("update_time")) {
             user_profile.set_update_time(rs->getString("update_time"));
        }
        return true;
    } catch (sql::SQLException& e) {
        uint64_t failed_uid = 0;
        try {
            if (!rs->isNull("uid")) failed_uid = rs->getUInt64("uid");
        } catch (...) {}
        LOG_ERROR("Mysql 结果解析失败 (可能在处理 uid={}): {}", failed_uid, e.what());
        return false;
    } catch (std::exception& e) {
         LOG_ERROR("结果解析时发生标准异常: {}", e.what());
         return false;
    } catch (...) {
         LOG_ERROR("结果解析时发生未知异常");
         return false;
    }
}

/**
 * 获取 uid 的联系人列表
 * @param uid 用户ID
 * @param contacts 联系人列表
 * @return 是否成功
 */
bool MysqlMgr::getContacts(uint64_t uid, std::vector<ContactInfo>& contacts) {
    auto conn = conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("MysqlMgr::getContacts() 获取数据库连接失败");
        return false;
    }
    Defer defer([this, &conn]() {
        conn_pool->returnConnection(std::move(conn));
    });
    try {
        // 因为添加联系人时是插入互为联系人的两条记录，所以这里不需要 user_id_a = ? OR user_id_b = ?
        std::string query = "SELECT uid, username, nickname, avatar_url, status, signature FROM users WHERE uid IN (SELECT user_id_b FROM contacts WHERE user_id_a = ?) ORDER BY username ASC";
        auto stmt = conn->prepareStatement(query);
        stmt->setUInt64(1, uid);
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
        while (rs->next()) {
            ContactInfo contact_info;
            contact_info.set_uid(rs->getUInt64("uid"));
            contact_info.set_username(rs->getString("username"));
            contact_info.set_nickname(rs->getString("nickname"));
            contact_info.set_avatar_url(rs->getString("avatar_url"));
            contact_info.set_status(static_cast<UserStatus>(rs->getInt("status")));
            contact_info.set_signature(rs->getString("signature"));
            contacts.push_back(contact_info);
        }
        return true;
    } catch (sql::SQLException& e) {
        LOG_ERROR("MysqlMgr::getContacts() 查询失败: {}", e.what());
        return false;
    } catch (std::exception& e) {
        LOG_ERROR("MysqlMgr::getContacts() 查询失败: {}", e.what());
        return false;
    } catch (...) {
        LOG_ERROR("MysqlMgr::getContacts() 查询失败: Unknown error");
        return false;
    }
}

/**
 * 搜索用户
 * @param keyword 关键词
 * @param users 用户列表
 * @return 是否成功
 */
bool MysqlMgr::searchUser(const std::string& keyword, std::vector<UserProfile>& users) {
    auto conn = conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("MysqlMgr::searchUser() 获取数据库连接失败");
        return false;
    }

    Defer defer([this, &conn]() {
        conn_pool->returnConnection(std::move(conn));
    });

    try {
        std::string query = "SELECT uid, username, nickname, avatar_url, email, phone, gender, signature, location, status, create_time, update_time FROM users WHERE username LIKE ? OR email LIKE ? ORDER BY username ASC";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, "%" + keyword + "%");
        stmt->setString(2, "%" + keyword + "%");
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
        while (rs->next()) {
            UserProfile user_profile;
            if (fromMysqlResult(rs.get(), user_profile)) {
                users.push_back(user_profile);
            }
        }
        return true;
    } catch (sql::SQLException& e) {
        LOG_ERROR("MysqlMgr::searchUser() 查询失败: {}", e.what());
        return false;
    } catch (std::exception& e) {
        LOG_ERROR("MysqlMgr::searchUser() 查询失败: {}", e.what());
        return false;
    } catch (...) {
        LOG_ERROR("MysqlMgr::searchUser() 查询失败: Unknown error");
        return false;
    }
}

/**
 * 发送联系人请求
 * @param requester_id 请求者ID
 * @param addressee_id 被请求者ID
 * @param message 请求消息
 * @param request_id 请求ID
 * @return 是否成功
 */
bool MysqlMgr::sendContactRequest(uint64_t requester_id, uint64_t addressee_id, const std::string& message, uint64_t& request_id) {
    try {
        auto conn = conn_pool->getConnection();
        if (!conn) {
            LOG_ERROR("无法获取数据库连接");
            return false;
        }

        // 检查是否已经是联系人
        if (areContacts(requester_id, addressee_id)) {
            LOG_ERROR("用户 {} 和 {} 已经是联系人", requester_id, addressee_id);
            return false;
        }

        // 检查是否已有待处理的请求
        if (hasPendingRequest(requester_id, addressee_id)) {
            LOG_ERROR("用户 {} 已经向 {} 发送了待处理的请求", requester_id, addressee_id);
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> stmt(conn->prepareStatement(
            "INSERT INTO contact_requests (requester_id, addressee_id, request_message) VALUES (?, ?, ?)"
        ));

        stmt->setUInt64(1, requester_id);
        stmt->setUInt64(2, addressee_id);
        stmt->setString(3, message);

        stmt->executeUpdate();

        // 获取插入的请求ID
        std::unique_ptr<sql::Statement> stmt2(conn->createStatement());
        std::unique_ptr<sql::ResultSet> rs(stmt2->executeQuery("SELECT LAST_INSERT_ID()"));
        if (rs->next()) {
            request_id = rs->getUInt64(1);
            return true;
        }

        return false;
    } catch (const sql::SQLException& e) {
        LOG_ERROR("发送联系人请求失败: {}", e.what());
        return false;
    }
}

bool MysqlMgr::handleContactRequest(uint64_t request_id, uint64_t user_id, ContactRequestStatus action) {
    try {
        auto conn = conn_pool->getConnection();
        if (!conn) {
            LOG_ERROR("无法获取数据库连接");
            return false;
        }

        // 开始事务
        conn->setAutoCommit(false);

        try {
            // 检查请求是否存在且属于该用户
            std::unique_ptr<sql::PreparedStatement> stmt(conn->prepareStatement(
                "SELECT requester_id, addressee_id FROM contact_requests WHERE id = ? AND addressee_id = ? AND status = ?"
            ));
            stmt->setUInt64(1, request_id);
            stmt->setUInt64(2, user_id);
            stmt->setInt(3, ContactRequestStatus::PENDING);

            std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
            if (!rs->next()) {
                conn->rollback();
                return false;
            }

            uint64_t requester_id = rs->getUInt64("requester_id");
            uint64_t addressee_id = rs->getUInt64("addressee_id");

            // 更新请求状态
            std::unique_ptr<sql::PreparedStatement> stmt2(conn->prepareStatement(
                "UPDATE contact_requests SET status = ? WHERE id = ?"
            ));
            stmt2->setInt(1, action);
            stmt2->setUInt64(2, request_id);
            stmt2->executeUpdate();

            // 如果接受请求，创建双向联系人关系
            if (action == ContactRequestStatus::ACCEPTED) {
                std::unique_ptr<sql::PreparedStatement> stmt3(conn->prepareStatement(
                    "INSERT INTO contacts (user_id_a, user_id_b) VALUES (?, ?), (?, ?)"
                ));
                stmt3->setUInt64(1, requester_id);
                stmt3->setUInt64(2, addressee_id);
                stmt3->setUInt64(3, addressee_id);
                stmt3->setUInt64(4, requester_id);
                stmt3->executeUpdate();
            }

            // 结束事务
            conn->commit();
            return true;
        } catch (const sql::SQLException& e) {
            conn->rollback();
            throw e;
        }
    } catch (const sql::SQLException& e) {
        LOG_ERROR("处理联系人请求失败: {}", e.what());
        return false;
    }
}

/**
 * 获取用户收到的联系人请求列表
 * @param user_id 用户ID
 * @param status 请求状态
 * @param requests 联系人请求列表
 * @return 是否成功
 */
bool MysqlMgr::getContactRequests(uint64_t user_id, ContactRequestStatus status, std::vector<ContactRequestInfo>& requests) {
    try {
        auto conn = conn_pool->getConnection();
        if (!conn) {
            LOG_ERROR("无法获取数据库连接");
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> stmt(conn->prepareStatement(
            "SELECT r.*, u.username, u.nickname, u.avatar_url "
            "FROM contact_requests r "
            "JOIN users u ON r.requester_id = u.uid "
            "WHERE r.addressee_id = ? AND r.status = ? "
            "ORDER BY r.created_at DESC"
        ));

        stmt->setUInt64(1, user_id);
        stmt->setInt(2, status);

        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
        while (rs->next()) {
            ContactRequestInfo request_info;
            if (fromMysqlResult(rs.get(), request_info)) {
                requests.push_back(request_info);
            }
        }

        return true;
    } catch (const sql::SQLException& e) {
        LOG_ERROR("获取联系人请求列表失败: {}", e.what());
        return false;
    }
}

/**
 * 获取一条具体的联系人请求详情
 * @param request_id 请求ID
 * @param request_info 联系人请求详情
 * @return 是否成功
 */
bool MysqlMgr::getContactRequestById(uint64_t request_id, ContactRequestInfo& request_info) {
    try {
        auto conn = conn_pool->getConnection();
        if (!conn) {
            LOG_ERROR("无法获取数据库连接");
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> stmt(conn->prepareStatement(
            "SELECT r.id, r.requester_id, r.addressee_id, r.request_message, r.status, r.created_at, r.updated_at, "
            "u.username, u.nickname, u.avatar_url "
            "FROM contact_requests r "
            "JOIN users u ON r.requester_id = u.uid "
            "WHERE r.id = ?"
        ));

        stmt->setUInt64(1, request_id);

        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
        if (rs->next()) {
            return fromMysqlResult(rs.get(), request_info);
        }

        return false;
    } catch (const sql::SQLException& e) {
        LOG_ERROR("获取联系人请求详情失败: {}", e.what());
        return false;
    }
}

/**
 * 检查两个用户是否是联系人
 * @param user_id_a 用户ID
 * @param user_id_b 用户ID
 * @return 是否是联系人
 */
bool MysqlMgr::areContacts(uint64_t user_id_a, uint64_t user_id_b) {
    try {
        auto conn = conn_pool->getConnection();
        if (!conn) {
            LOG_ERROR("无法获取数据库连接");
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> stmt(conn->prepareStatement(
            "SELECT COUNT(*) FROM contacts WHERE user_id_a = ? AND user_id_b = ?" // 因为添加联系人时是插入互为联系人的两条记录，所以这里不需要 user_id_a = ? OR user_id_b = ?
        ));

        stmt->setUInt64(1, user_id_a);
        stmt->setUInt64(2, user_id_b);

        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
        if (rs->next()) {
            return rs->getInt(1) > 0;
        }

        return false;
    } catch (const sql::SQLException& e) {
        LOG_ERROR("检查联系人关系失败: {}", e.what());
        return false;
    }
}

/**
 * 检查用户是否收到待处理的联系人请求
 * @param requester_id 请求者ID
 * @param addressee_id 被请求者ID
 * @return 是否收到待处理的联系人请求
 */
bool MysqlMgr::hasPendingRequest(uint64_t requester_id, uint64_t addressee_id) {
    try {
        auto conn = conn_pool->getConnection();
        if (!conn) {
            LOG_ERROR("无法获取数据库连接");
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> stmt(conn->prepareStatement(
            "SELECT COUNT(*) FROM contact_requests WHERE requester_id = ? AND addressee_id = ? AND status = ?"
        ));

        stmt->setUInt64(1, requester_id);
        stmt->setUInt64(2, addressee_id);
        stmt->setInt(3, ContactRequestStatus::PENDING);

        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
        if (rs->next()) {
            return rs->getInt(1) > 0;
        }

        return false;
    } catch (const sql::SQLException& e) {
        LOG_ERROR("检查待处理请求失败: {}", e.what());
        return false;
    }
}

bool MysqlMgr::fromMysqlResult(const sql::ResultSet* rs, ContactRequestInfo& request_info) {
    try {
        request_info.set_request_id(rs->getUInt64("id"));
        request_info.set_requester_id(rs->getUInt64("requester_id"));
        request_info.set_addressee_id(rs->getUInt64("addressee_id"));
        request_info.set_requester_name(rs->getString("username"));
        request_info.set_requester_nickname(rs->getString("nickname"));
        request_info.set_requester_avatar(rs->getString("avatar_url"));
        request_info.set_request_message(rs->getString("request_message"));
        request_info.set_status(static_cast<ContactRequestStatus>(rs->getInt("status")));
        request_info.set_created_at(rs->getString("created_at"));
        request_info.set_updated_at(rs->getString("updated_at"));
        return true;
    } catch (const sql::SQLException& e) {
        LOG_ERROR("从MySQL结果集转换联系人请求信息失败: {}", e.what());
        return false;
    }
}
