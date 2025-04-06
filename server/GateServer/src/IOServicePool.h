//
// Created by wellwei on 2025/4/6.
//

#ifndef GATESERVER_IOSERVICEPOOL_H
#define GATESERVER_IOSERVICEPOOL_H
#include "global.h"


class IOServicePool : public Singleton<IOServicePool> {
    friend class Singleton<IOServicePool>;
public:
    using IOService = boost::asio::io_context;
    using Work = boost::asio::executor_work_guard<IOService::executor_type>;

private:
    IOServicePool(size_t = std::max(1u, std::thread::hardware_concurrency()));
};


#endif //GATESERVER_IOSERVICEPOOL_H
