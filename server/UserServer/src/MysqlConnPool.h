#pragma once

#include <queue>
#include <mysql/jdbc.h>

class MysqlConnPool {
public:
    MysqlConnPool(std::string url, std::string user, std::string password, std::string db,
                  const int poolSize = 10);

    ~MysqlConnPool();

    std::unique_ptr<sql::Connection> getConnection();

    void returnConnection(std::unique_ptr<sql::Connection> conn);

    void addConnection();

    void close();

private:
    std::string _url;
    std::string _user;
    std::string _password;
    std::string _db;
    int _poolSize;
    std::queue<std::unique_ptr<sql::Connection>> _connectionPool;
    std::mutex _mutex;
    std::condition_variable _condition;
    std::atomic_bool _stop;
};