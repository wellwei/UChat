//
// Created by wellwei on 2025/4/1.
//

#ifndef GATESERVER_SINGLETON_H
#define GATESERVER_SINGLETON_H

#include <memory>
#include <iostream>

template<typename T>
class Singleton {
protected:
    Singleton() = default;

    Singleton(const Singleton<T> &) = delete;

    Singleton &operator=(const Singleton<T> &) = delete;

public:
    static std::shared_ptr<T> getInstance() {
        static std::shared_ptr<T> instance(new T());
        return instance;
    }

    void printAddress() {
        std::cout << "Singleton instance address: " << this << std::endl;
    }

    ~Singleton() = default;
};

#endif //GATESERVER_SINGLETON_H
