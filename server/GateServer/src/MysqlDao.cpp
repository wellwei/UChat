//
// Created by wellwei on 2025/4/10.
//

#include "MysqlDao.h"
#include "ConfigMgr.h"

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

int MysqlDao::RegisterUser(const std::string &name, const std::string &email, const std::string &password) {
    auto conn = _conn_pool->getConnection();
    if (!conn) {
        std::cerr << "Failed to get connection from pool" << std::endl;
        return -1;
    }

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
            if (result > 0) {
                std::cout << "User registered successfully" << std::endl;
            } else {
                std::cerr << "Failed to register user, error code: " << result << std::endl;
            }
            _conn_pool->returnConnection(std::move(conn)); // 将连接放回连接池
            return result; // 返回存储过程的结果
        }

        // 如果没有结果，返回 -1
        std::cerr << "No result from stored procedure" << std::endl;
        _conn_pool->returnConnection(std::move(conn)); // 将连接放回连接池
        return -1;
    }
    catch (sql::SQLException &e) {
        _conn_pool->returnConnection(std::move(conn));
        std::cerr << "SQLException: " << e.what() << std::endl;
        std::cerr << "MySQL error code: " << e.getErrorCode() << std::endl;
        std::cerr << "SQLState: " << e.getSQLState() << std::endl;
        return -1;
    }
}
