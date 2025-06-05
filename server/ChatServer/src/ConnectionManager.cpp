#include "ConnectionManager.h"
#include "Logger.h"

void ConnectionManager::addConnection(const TcpConnectionPtr& conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_[conn->id()] = conn;
}

void ConnectionManager::removeConnection(const std::string& conn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = connections_.find(conn_id);
    if (it != connections_.end()) {
        // 如果是已认证的用户，从用户连接映射中也移除
        if (it->second->isAuthenticated()) {
            auto uid = it->second->uid();
            auto user_it = user_connections_.find(uid);
            
            // 只有在映射的连接与当前连接相同时才移除
            if (user_it != user_connections_.end() && user_it->second->id() == conn_id) {
                LOG_INFO("移除用户连接映射: uid={}", uid);
                user_connections_.erase(uid);
            }
        }
        
        connections_.erase(it);
    }
}

TcpConnectionPtr ConnectionManager::findConnection(const std::string& conn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connections_.find(conn_id);
    if (it != connections_.end()) {
        return it->second;
    }
    return nullptr;
}

TcpConnectionPtr ConnectionManager::findConnectionByUid(uint64_t uid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = user_connections_.find(uid);
    if (it != user_connections_.end()) {
        return it->second;
    }
    return nullptr;
}

void ConnectionManager::addUserConnection(uint64_t uid, const TcpConnectionPtr& conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if user is already connected
    auto it = user_connections_.find(uid);
    if (it != user_connections_.end() && it->second != conn) {
        // User is already connected with a different connection
        // Close the existing connection
        it->second->close();
    }
    
    // Add new user connection
    user_connections_[uid] = conn;
}

void ConnectionManager::removeUserConnection(uint64_t uid) {
    std::lock_guard<std::mutex> lock(mutex_);
    user_connections_.erase(uid);
}

void ConnectionManager::closeAllConnections() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& conn : connections_) {
        conn.second->close();
    }
    
    connections_.clear();
    user_connections_.clear();
}