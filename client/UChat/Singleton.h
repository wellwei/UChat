//
// Created by wellwei on 2025/3/28.
//

#ifndef UCHAT_SINGLETON_H
#define UCHAT_SINGLETON_H

#include <memory>
#include <iostream>

// 单例模式模板类（线程安全）
template<typename T>
class Singleton {
protected:
    Singleton() = default;

    Singleton(const Singleton<T> &) = delete;

    Singleton &operator=(const Singleton<T> &) = delete;

    ~Singleton() = default;

public:
    // 获取单例实例，静态局部变量保证线程安全
    static std::shared_ptr<T> getInstance() {
        static std::shared_ptr<T> instance(new T());
        return instance;
    }

    void printAddress() {
        std::cout << "Singleton instance address: " << this << std::endl;
    }
};

#endif //UCHAT_SINGLETON_H
