#pragma once

#include <functional>
#include <queue>
#include <sw/redis++/redis++.h>

class RedisConnPool {
public:
    RedisConnPool(std::string host, int port, const std::string &password, int db_index, size_t pool_size);

    ~RedisConnPool();

    std::shared_ptr<sw::redis::Redis> getConnection();

    void returnConnection(std::shared_ptr<sw::redis::Redis> conn);

    void close();

private:
    std::atomic_bool _stop;
    size_t _pool_size;
    std::string _host;
    int _port;
    std::string _password;
    int _db_index;
    std::queue<std::shared_ptr<sw::redis::Redis>> _connections;
    std::mutex _mutex;
    std::condition_variable _cond_var;
};
