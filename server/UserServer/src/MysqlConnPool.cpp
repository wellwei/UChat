//
// Created by wellwei on 2025/4/10.
//

#include "MysqlConnPool.h"
#include "Logger.h"

MysqlConnPool::MysqlConnPool(std::string url, std::string user, std::string password, std::string db, const int poolSize)
        : _url(std::move(url)),
          _user(std::move(user)), _password(std::move(password)),
          _db(std::move(db)),
          _poolSize(poolSize), _stop(false) {
    try {
        for (int i = 0; i < _poolSize; ++i) {
            auto driver = sql::mysql::get_mysql_driver_instance();
            std::unique_ptr<sql::Connection> conn(driver->connect(_url, _user, _password));
            conn->setSchema(_db);
            _connectionPool.push(std::move(conn));
        }
        LOG_INFO("MySQL connection pool created with size: {}", _poolSize);
    }
    catch (sql::SQLException &e) {
        LOG_CRITICAL("Error creating MySQL connection pool: {}", e.what());
        throw;
    }
}

MysqlConnPool::~MysqlConnPool() {
    std::unique_lock<std::mutex> lock(_mutex);
    close();
    _connectionPool = std::queue<std::unique_ptr<sql::Connection>>();
}

std::unique_ptr<sql::Connection> MysqlConnPool::getConnection() {
    std::unique_lock<std::mutex> lock(_mutex);
    while (_connectionPool.empty() && !_stop) {
        _condition.wait(lock);
    }
    if (_stop) {
        return nullptr;
    }
    auto conn = std::move(_connectionPool.front());
    _connectionPool.pop();
    return conn;
}

void MysqlConnPool::returnConnection(std::unique_ptr<sql::Connection> conn) {
    if (!conn) {
        return;
    }

    std::unique_lock<std::mutex> lock(_mutex);
    if (_stop) {
        return;
    }

    _connectionPool.push(std::move(conn));
    _condition.notify_one();
}

void MysqlConnPool::close() {
    _stop = true;
    _condition.notify_all();
}

void MysqlConnPool::addConnection() {

}
