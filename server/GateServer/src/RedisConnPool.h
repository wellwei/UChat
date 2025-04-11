//
// Created by wellwei on 2025/4/9.
//

#ifndef GATESERVER_REDISCONNPOOL_H
#define GATESERVER_REDISCONNPOOL_H

#include "global.h"

class RedisConnPool {
public:
    RedisConnPool(size_t pool_size, std::string host, int port, const std::string &password);

    ~RedisConnPool();

    std::shared_ptr<sw::redis::Redis> getConnection();

    void returnConnection(std::shared_ptr<sw::redis::Redis> conn);

    void close();

private:
    std::atomic_bool _stop;
    size_t _pool_size;
    std::string _host;
    int _port;
    std::queue<std::shared_ptr<sw::redis::Redis>> _connections;
    std::mutex _mutex;
    std::condition_variable _cond_var;
};


#endif //GATESERVER_REDISCONNPOOL_H
