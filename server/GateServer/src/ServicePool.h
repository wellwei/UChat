//
// Created by wellwei on 2025/4/6.
//

#ifndef GATESERVER_SERVICEPOOL_H
#define GATESERVER_SERVICEPOOL_H

#include "global.h"
#include "Singleton.h"

class ServicePool : public Singleton<ServicePool> {
    friend class Singleton<ServicePool>;

public:
    ~ServicePool();

    ServicePool(const ServicePool &) = delete;

    ServicePool &operator=(const ServicePool &) = delete;

    boost::asio::io_context &getNextService();

    void stop();

private:
    ServicePool(size_t = std::max(1u, std::thread::hardware_concurrency()));

    using Service = boost::asio::io_context;
    using WorkGuard = boost::asio::executor_work_guard<Service::executor_type>;
    using WorkGuardPtr = std::shared_ptr<WorkGuard>;
    using ServicePtr = std::shared_ptr<Service>;

    std::vector<ServicePtr> _services;
    std::vector<WorkGuardPtr> _work_guards;
    std::vector<std::thread> _threads;
    std::atomic_size_t _next_service_index{0};
};


#endif //GATESERVER_SERVICEPOOL_H
