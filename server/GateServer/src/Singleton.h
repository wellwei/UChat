//
// Created by wellwei on 2025/4/1.
//

#ifndef GATESERVER_SINGLETON_H
#define GATESERVER_SINGLETON_H

#include <memory>

template <typename T>
class Singleton {
protected:
    Singleton() = default;
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

public:
    ~Singleton() = default;
    static std::shared_ptr<T> getInstance() {
        static std::shared_ptr<T> instance(new T());
        return instance;
    }
};

#endif //GATESERVER_SINGLETON_H
