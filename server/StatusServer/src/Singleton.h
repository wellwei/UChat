#pragma once

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


