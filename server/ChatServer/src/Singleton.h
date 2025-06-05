#pragma once

template <typename T>
class Singleton {
protected:
    Singleton() = default;
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

public:
    ~Singleton() = default;
    static T& getInstance() {
        static T instance;
        return instance;
    }
};


