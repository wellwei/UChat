//
// Created by wellwei on 2025/4/7.
//

#ifndef GATESERVER_GRPCSTUBPOOL_H
#define GATESERVER_GRPCSTUBPOOL_H

#include "global.h"
#include "message.grpc.pb.h"
#include "Singleton.h"
#include "Logger.h"

// gRPC 存根模版类
template<typename T>
class GrpcStubPool {
public:
    GrpcStubPool(size_t pool_size, std::string server_address)
            : _pool_size(pool_size),
              _server_address(std::move(server_address)),
              _stop(false) {
        _channel = grpc::CreateChannel(_server_address, grpc::InsecureChannelCredentials());
        if (!_channel) {
            LOG_CRITICAL("Failed to create gRPC channel");
            throw std::runtime_error("Failed to create gRPC channel");
        }
        for (size_t i = 0; i < _pool_size; ++i) {
            auto stub = T::NewStub(_channel);
            _stubs.push(std::move(stub));
        }
        LOG_INFO("gRPC stub pool initialized with {} stubs", _pool_size);
    }

    ~GrpcStubPool() {
        std::unique_lock<std::mutex> lock(_mutex);
        close();
        _stubs = std::queue<std::unique_ptr<typename T::Stub>>();   // 清空存根队列
    }

    std::unique_ptr<typename T::Stub> getStub() {
        std::unique_lock<std::mutex> lock(_mutex);
        _cond_var.wait(lock, [this] {
            return !_stubs.empty() || _stop;
        });
        if (_stop) {
            return nullptr; // 如果停止了，返回空指针
        }

        auto stub = std::move(_stubs.front());
        _stubs.pop();
        return stub; // 返回存根
    }

    void returnStub(std::unique_ptr<typename T::Stub> stub) {
        if (!stub) {
            LOG_WARN("Attempted to return a null stub");
            return;
        }
        std::unique_lock<std::mutex> lock(_mutex);
        if (_stop) {
            return; // 如果停止了，直接返回
        }

        _stubs.push(std::move(stub)); // 将存根放回队列
        _cond_var.notify_one(); // 通知一个等待的线程
    }

    void close() {
        _stop = true;
        _cond_var.notify_all(); // 通知所有等待的线程
    }

private:
    std::atomic_bool _stop;
    size_t _pool_size;
    std::string _server_address;
    std::shared_ptr<grpc::Channel> _channel;
    std::queue<std::unique_ptr<typename T::Stub>> _stubs;
    std::mutex _mutex;
    std::condition_variable _cond_var;
};


#endif //GATESERVER_GRPCSTUBPOOL_H
