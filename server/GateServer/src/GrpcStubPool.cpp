//
// Created by wellwei on 2025/4/7.
//

#include "GrpcStubPool.h"

#include <utility>

GrpcStubPool::GrpcStubPool(size_t pool_size, std::string server_address)
        : _pool_size(pool_size),
          _server_address(std::move(server_address)),
          _stop(false) {
    _channel = grpc::CreateChannel(_server_address, grpc::InsecureChannelCredentials());
    if (!_channel) {
        throw std::runtime_error("Failed to create gRPC channel");
    }
    for (size_t i = 0; i < _pool_size; ++i) {
        auto stub = message::VerifyService::NewStub(_channel);
        _stubs.push(std::move(stub));
    }
}

GrpcStubPool::~GrpcStubPool() {
    std::unique_lock<std::mutex> lock(_mutex);
    close();
    _stubs = std::queue<std::unique_ptr<message::VerifyService::Stub>>();   // 清空存根队列
}

std::unique_ptr<message::VerifyService::Stub> GrpcStubPool::getStub() {
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

void GrpcStubPool::returnStub(std::unique_ptr<message::VerifyService::Stub> stub) {
    if (!stub) {
        return;
    }
    std::unique_lock<std::mutex> lock(_mutex);
    if (_stop) {
        return; // 如果停止了，直接返回
    }

    _stubs.push(std::move(stub)); // 将存根放回队列
    _cond_var.notify_one(); // 通知一个等待的线程
}

void GrpcStubPool::close() {
    _stop = true;
    _cond_var.notify_all(); // 通知所有等待的线程
}