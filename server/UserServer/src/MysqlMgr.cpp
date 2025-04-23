#include "MysqlMgr.h"
#include "util.h"
#include "ConfigMgr.h"  
#include "proto_generated/user_service.pb.h"
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

bool MysqlMgr::findById(uint64_t uid, UserProfile& user_profile) {
    auto conn = conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("MysqlMgr::findById() 获取数据库连接失败");
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
            return fromMysqlResult(rs.get(), user_profile);
        }
        return false;
    } catch (sql::SQLException& e) {
        LOG_ERROR("MysqlMgr::findById() 查询失败: {}", e.what());
        return false;
    }
}

bool MysqlMgr::findByUsername(const std::string& username, UserProfile& user_profile) {
    auto conn = conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("MysqlMgr::findByUsername() 获取数据库连接失败");
        return false;
    }
    Defer defer([this, &conn]() {
        conn_pool->returnConnection(std::move(conn));
    });
    try {
        std::string query = "SELECT uid, username, nickname, avatar_url, email, phone, gender, signature, location, status, create_time, update_time FROM users WHERE username = ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
        if (rs->next()) {
            return fromMysqlResult(rs.get(), user_profile);
        }
        return false;
    } catch (sql::SQLException& e) {
        LOG_ERROR("MysqlMgr::findByUsername() 查询失败: {}", e.what());
        return false;
    }
}

bool MysqlMgr::updateById(uint64_t uid, const UserProfile& user_profile) {
    auto conn = conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("MysqlMgr::updateById() 获取数据库连接失败");
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
        stmt->executeUpdate();
        return true;
    } catch (sql::SQLException& e) {
        LOG_ERROR("MysqlMgr::updateById() 更新失败: {}", e.what());
        return false;
    }
}

bool MysqlMgr::updateByUsername(const std::string& username, const UserProfile& user_profile) {
    auto conn = conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("MysqlMgr::updateByUsername() 获取数据库连接失败");
        return false;
    }
    Defer defer([this, &conn]() {
        conn_pool->returnConnection(std::move(conn));
    });
    try {
        std::string query = "UPDATE users SET username = ?, nickname = ?, avatar_url = ?, email = ?, phone = ?, gender = ?, signature = ?, location = ?, status = ?, update_time = NOW() WHERE username = ?";
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
        stmt->setString(10, username);
        stmt->executeUpdate();
        return true;
    } catch (sql::SQLException& e) {
        LOG_ERROR("MysqlMgr::updateByUsername() 更新失败: {}", e.what());
        return false;
    }
}

bool MysqlMgr::deleteById(uint64_t uid) {
    auto conn = conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("MysqlMgr::deleteById() 获取数据库连接失败");
        return false;
    }
    Defer defer([this, &conn]() {
        conn_pool->returnConnection(std::move(conn));
    });
    try {
        std::string query = "DELETE FROM users WHERE uid = ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setUInt64(1, uid);
        stmt->executeUpdate();
        return true;
    } catch (sql::SQLException& e) {
        LOG_ERROR("MysqlMgr::deleteById() 删除失败: {}", e.what());
        return false;
    }
}   

bool MysqlMgr::deleteByUsername(const std::string& username) {
    auto conn = conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("MysqlMgr::deleteByUsername() 获取数据库连接失败");
        return false;
    }
    Defer defer([this, &conn]() {
        conn_pool->returnConnection(std::move(conn));
    });
    try {
        std::string query = "DELETE FROM users WHERE username = ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, username);
        stmt->executeUpdate();
        return true;
    } catch (sql::SQLException& e) {
        LOG_ERROR("MysqlMgr::deleteByUsername() 删除失败: {}", e.what());
        return false;
    }   
}

bool MysqlMgr::verifyCredentials(const std::string& handle, const std::string& password, uint64_t &uid, UserProfile& user_profile) {
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
        std::string query = "SELECT uid, username, nickname, avatar_url, email, phone, gender, signature, location, status, create_time, update_time FROM users WHERE username = ? AND password = ?";
        auto stmt = conn->prepareStatement(query);
        stmt->setString(1, handle);
        stmt->setString(2, hashed_password);
        
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
        if (rs->next()) {
            uid = rs->getUInt64(1);
            return getUserProfile(uid, user_profile);   // 尝试从缓存中获取用户信息
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

    auto cache_value = redis_mgr->get(cache_key);
    if (cache_value) {
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
