//
// Created by wellwei on 2025/3/28.
//

#ifndef UCHAT_SINGLETON_H
#define UCHAT_SINGLETON_H

#include <memory>
#include <iostream>

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


#endif //UCHAT_SINGLETON_H
