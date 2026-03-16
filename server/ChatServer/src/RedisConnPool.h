//
// Created by wellwei on 2025/4/9.
//

#ifndef CHATSERVER_REDISCONNPOOL_H
#define CHATSERVER_REDISCONNPOOL_H

#include "Logger.h"
#include <sw/redis++/redis++.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

class RedisConnPool {
public:
    RedisConnPool(size_t pool_size, std::string host, int port);

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


#endif //CHATSERVER_REDISCONNPOOL_H
