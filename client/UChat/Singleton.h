//
// Created by wellwei on 2025/3/28.
//

#ifndef UCHAT_SINGLETON_H
#define UCHAT_SINGLETON_H

#include <memory>
#include <iostream>

// 单例模式饿汉式模板类（线程安全）
template<typename T>
class Singleton {
protected:
    Singleton() = default;

    virtual ~Singleton() = default;

public:
    Singleton(const Singleton &) = delete;

    Singleton &operator=(const Singleton &) = delete;

    Singleton(Singleton &&) = delete;

    Singleton &operator=(Singleton &&) = delete;

    // 获取单例实例
    static T &getInstance() {
        return instance;
    }

    void printAddress() {
        std::cout << "Singleton instance address: " << this << std::endl;
    }

private:
    static T instance; // 静态实例
};

template<typename T>
T Singleton<T>::instance; // 静态实例初始化

#endif //UCHAT_SINGLETON_H
