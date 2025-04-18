//
// Created by wellwei on 2025/4/10.
//

#include "MysqlDao.h"
#include "ConfigMgr.h"
#include "Logger.h"
#include "util.h"

MysqlDao::MysqlDao() {
    auto cfm = *ConfigMgr::getInstance();
    std::string host = cfm["Mysql"]["host"];
    std::string port = cfm["Mysql"]["port"];
    std::string user = cfm["Mysql"]["user"];
    std::string password = cfm["Mysql"]["password"];
    std::string database = cfm["Mysql"]["database"];
    _conn_pool = std::make_unique<MysqlConnPool>(host + ":" + port, user, password, database, 8);
}

MysqlDao::~MysqlDao() {
    _conn_pool->close();
}

int MysqlDao::registerUser(const std::string &name, const std::string &email, const std::string &password) {
    auto conn = _conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get connection from pool");
        return -1;
    }
    Defer defer([this, &conn]() {
        _conn_pool->returnConnection(std::move(conn)); // 将连接放回连接池
    });

    try {
        // 准备 调用存储过程
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement("CALL register_user(?, ?, ?, @result)"));
        pstmt->setString(1, name);
        pstmt->setString(2, email);
        pstmt->setString(3, password);
        // 执行存储过程
        pstmt->execute();

        // 获取结果
        std::unique_ptr<sql::Statement> stmtResult(conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
        if (res->next()) {
            int result = res->getInt("result");
            if (result == 0) {
                LOG_ERROR("CALL register_user failed");
            }
            return result; // 返回存储过程的结果
        }

        LOG_ERROR("CALL register_user failed: No result");
        // 如果没有结果，返回 0
        return 0;
    }
    catch (sql::SQLException &e) {
        LOG_ERROR("SQLException: {}", e.what());
        LOG_ERROR("MySQL error code: {} SQLState: {}", e.getErrorCode(), e.getSQLState());
        return 0;
    }
}

int MysqlDao::getUidByEmail(const std::string &email) {
    auto conn = _conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get connection from pool");
        return -1;
    }
    Defer defer([this, &conn]() {
        _conn_pool->returnConnection(std::move(conn)); // 将连接放回连接池
    });

    try {
        // 准备 SQL 查询
        std::unique_ptr<sql::PreparedStatement> pstmt(
                conn->prepareStatement("SELECT uid FROM user_info WHERE email = ?"));
        pstmt->setString(1, email);

        // 执行查询
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            int uid = res->getInt("uid");
            return uid; // 返回用户 ID
        }

        // 如果没有结果，返回 -1
        return -1;
    }
    catch (sql::SQLException &e) {
        LOG_ERROR("SQLException: {}", e.what());
        LOG_ERROR("MySQL error code: {} SQLState: {}", e.getErrorCode(), e.getSQLState());
        return -1;
    }
}

bool MysqlDao::resetPassword(int uid, const std::string &password) {
    auto conn = _conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get connection from pool");
        return false;
    }
    Defer defer([this, &conn]() {
        _conn_pool->returnConnection(std::move(conn)); // 将连接放回连接池
    });

    try {
        // 准备 SQL 更新
        std::unique_ptr<sql::PreparedStatement> pstmt(
                conn->prepareStatement("UPDATE user_info SET password = ? WHERE uid = ?"));
        pstmt->setString(1, password);
        pstmt->setInt(2, uid);

        // 执行更新
        int rowsAffected = pstmt->executeUpdate();

        return rowsAffected > 0; // 如果更新成功，返回 true
    }
    catch (sql::SQLException &e) {
        LOG_ERROR("SQLException: {}", e.what());
        LOG_ERROR("MySQL error code: {} SQLState: {}", e.getErrorCode(), e.getSQLState());
        return false; // 密码重置失败
    }
}

int MysqlDao::getUidByUsername(const std::string &username) {
    auto conn = _conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get connection from pool");
        return -1;
    }
    Defer defer([this, &conn]() {
        _conn_pool->returnConnection(std::move(conn)); // 将连接放回连接池
    });

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
                conn->prepareStatement("SELECT uid FROM user_info WHERE username = ?"));
        pstmt->setString(1, username);

        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            int uid = res->getInt("uid");
            return uid; // 返回用户 ID
        }
        // 如果没有结果，返回 -1
        return -1;
    }
    catch (sql::SQLException &e) {
        LOG_ERROR("SQLException: {}", e.what());
        LOG_ERROR("MySQL error code: {} SQLState: {}", e.getErrorCode(), e.getSQLState());
        return -1;
    }
}

bool MysqlDao::checkPassword(int uid, const std::string &password) {
    auto conn = _conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get connection from pool");
        return -1;
    }
    Defer defer([this, &conn]() {
        _conn_pool->returnConnection(std::move(conn)); // 将连接放回连接池
    });

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "SELECT password FROM user_info WHERE uid = ?"));
        pstmt->setInt(1, uid);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            std::string db_password = res->getString("password");
            return db_password == password; // 比较密码
        }

        // 如果没有结果，返回 false
        return false;
    }
    catch (sql::SQLException &e) {
        LOG_ERROR("SQLException: {}", e.what());
        LOG_ERROR("MySQL error code: {} SQLState: {}", e.getErrorCode(), e.getSQLState());
        return false; // 密码检查失败
    }
}

UserInfo MysqlDao::getUserInfo(int uid) {
    auto conn = _conn_pool->getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get connection from pool");
        return {}; // 返回空的 UserInfo 对象
    }
    Defer defer([this, &conn]() {
        _conn_pool->returnConnection(std::move(conn)); // 将连接放回连接池
    });

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
                conn->prepareStatement("SELECT * FROM user_info WHERE uid = ?"));
        pstmt->setInt(1, uid);

        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            UserInfo userInfo;
            userInfo.uid = res->getInt("uid");
            userInfo.username = res->getString("username");
            userInfo.email = res->getString("email");
            userInfo.password = res->getString("password");
            return userInfo; // 返回用户信息
        }

        // 如果没有结果，返回空的 UserInfo 对象
        return {};
    }
    catch (sql::SQLException &e) {
        LOG_ERROR("SQLException: {}", e.what());
        LOG_ERROR("MySQL error code: {} SQLState: {}", e.getErrorCode(), e.getSQLState());
        return {}; // 返回空的 UserInfo 对象
    }
}
