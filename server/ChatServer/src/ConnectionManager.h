#pragma once

#include "TcpConnection.h"
#include <unordered_map>
#include <mutex>
#include <memory>
#include <string>

class ConnectionManager {
public:
    ConnectionManager() = default;
    ~ConnectionManager() = default;

    void addConnection(const TcpConnectionPtr& conn);

    void removeConnection(const std::string& conn_id);

    TcpConnectionPtr findConnection(const std::string& conn_id);

    TcpConnectionPtr findConnectionByUid(uint64_t uid);

    void addUserConnection(uint64_t uid, const TcpConnectionPtr& conn);

    void removeUserConnection(uint64_t uid);

    const std::unordered_map<std::string, TcpConnectionPtr>& getAllConnections() const {
        return connections_;
    }

    const std::unordered_map<uint64_t, TcpConnectionPtr>& getAllUserConnections() const {
        return user_connections_;
    }

    void closeAllConnections();

    size_t connectionCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return connections_.size();
    }

private:
    std::unordered_map<std::string, TcpConnectionPtr> connections_;
    std::unordered_map<uint64_t, TcpConnectionPtr> user_connections_;
    mutable std::mutex mutex_;
}; 