#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
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
    using WorkGuardPtr = std::unique_ptr<WorkGuard>;
    using ServicePtr = std::shared_ptr<Service>;

    std::vector<ServicePtr> _services;
    std::vector<WorkGuardPtr> _work_guards;
    std::vector<std::thread> _threads;
    std::atomic_size_t _next_service_index{0};
};