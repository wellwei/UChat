#include "ChatManager.h"
#include <QDebug>

ChatManager::~ChatManager() {
    stopHeartbeatMonitor();
    disconnect();
}

bool ChatManager::connectToServer(const QString &host, quint16 port) {
    // 保存连接信息，用于重连
    m_lastHost = host;
    m_lastPort = port;
    
    auto connection = std::make_shared<ChatConnection>(this);
    
    // 连接信号和槽
    QObject::connect(connection.get(), &ChatConnection::connected, this, [this]() {
        qDebug() << "ChatManager: 连接到服务器成功";
        m_reconnectAttempts = 0;  // 重置重连计数
        emit connected();
    });
    
    QObject::connect(connection.get(), &ChatConnection::disconnected, this, [this]() {
        qDebug() << "ChatManager: 与服务器断开连接";
        stopHeartbeatMonitor();
        
        // 如果设置了自动重连，开始重连
        if (m_autoReconnect && m_reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
            m_reconnectTimer.start(RECONNECT_INTERVAL);
        }
        
        emit disconnected();
    });
    
    QObject::connect(connection.get(), &ChatConnection::error, this, [this](const QString &errorMessage) {
        qDebug() << "ChatManager: 连接错误: " << errorMessage;
        emit error(errorMessage);
    });
    
    QObject::connect(connection.get(), &ChatConnection::authenticated, this, [this]() {
        qDebug() << "ChatManager: 认证成功";

        // 认证成功，关闭重连定时器, 重置重连计数
        m_reconnectTimer.stop();
        m_reconnectAttempts = 0;

        startHeartbeatMonitor();  // 认证成功后开始心跳
        emit authenticated();
    });
    
    QObject::connect(connection.get(), &ChatConnection::authenticationFailed, this, [this](int errorCode, const QString &errorMessage) {
        qDebug() << "ChatManager: 认证失败, 代码: " << errorCode << ", 消息: " << errorMessage;
        emit authenticationFailed(errorCode, errorMessage);
    });
    
    QObject::connect(connection.get(), &ChatConnection::messageReceived, this, [this](uint64_t fromUid, const QString &content, const QString &msgType, qint64 timestamp) {
        qDebug() << "ChatManager: 收到消息, 来自: " << fromUid << ", 类型: " << msgType << ", 内容: " << content << ", 时间戳: " << timestamp;
        emit messageReceived(fromUid, content, msgType, timestamp);
    });
    
    QObject::connect(connection.get(), &ChatConnection::messageSent, this, [this](uint64_t toUid, const QString &content, bool success, const QString &message) {
        qDebug() << "ChatManager: 消息发送状态, 目标: " << toUid << ", 内容: " << content << ", 成功: " << success << ", 消息: " << message;
        emit messageSent(toUid, content, success, message);
    });
    
    // 存储连接对象
    m_connection = connection;
    
    // 设置自动重连为true，但还没开始心跳
    m_autoReconnect = true;
    
    // 连接到服务器并返回连接请求是否成功发起
    bool requestSucceeded = false;
    try {
    m_connection->connectToServer(host, port);
        requestSucceeded = true;
        qDebug() << "ChatManager: 连接请求已发起，等待建立连接...";
    } catch (const std::exception &e) {
        qDebug() << "ChatManager: 连接请求发起失败：" << e.what();
        requestSucceeded = false;
    }
    
    return requestSucceeded;
}

void ChatManager::disconnect() {
    // 停止心跳和重连
    stopHeartbeatMonitor();
    m_reconnectTimer.stop();
    m_autoReconnect = false;
    
    if (m_connection) {
        m_connection->disconnect();
        m_connection.reset();
    }
}

bool ChatManager::authenticate(uint64_t uid, const QString &token) {
    // 保存认证信息，用于重连
    m_lastUid = uid;
    m_lastToken = token;
    
    if (!m_connection) {
        qCritical() << "认证失败: 连接对象为空";
        emit error("认证失败: 未创建连接，请先连接到服务器");
        return false;
    }
    
    if (!m_connection->isConnected()) {
        qCritical() << "认证失败: 未连接到服务器 (UID:" << uid << ")";
        emit error("认证失败: 未连接到服务器，请确保网络连接正常");
        return false;
    }
    
    try {
        // 发送认证请求
        qInfo() << "发起认证请求 (UID:" << uid << ")";
        m_connection->authenticate(uid, token);
        return true;  // 认证请求已发起
    } catch (const std::exception &e) {
        qCritical() << "认证请求发起失败: " << e.what() << " (UID:" << uid << ")";
        emit error(QString("认证请求发起失败: %1").arg(e.what()));
        return false;
    }
}

void ChatManager::sendMessage(uint64_t toUid, const QString &content, const QString &msgType) {
    if (!m_connection || !m_connection->isAuthenticated()) {
        qDebug() << "ChatManager: 未认证，无法发送消息";
        emit error("未认证，无法发送消息");
        return;
    }
    
    m_connection->sendChatMessage(toUid, content, msgType);
}

bool ChatManager::isConnected() const {
    return m_connection && m_connection->isConnected();
}

bool ChatManager::isAuthenticated() const {
    return m_connection && m_connection->isAuthenticated();
}

void ChatManager::startHeartbeatMonitor() {
    // 设置心跳定时器
    connect(&m_heartbeatTimer, &QTimer::timeout, this, [this]() {
        if (m_connection && m_connection->isConnected()) {
            qDebug() << "ChatManager: 发送心跳";
            m_connection->sendHeartbeat();
            
            // 启动超时定时器
            m_heartbeatTimeoutTimer.start(HEARTBEAT_TIMEOUT);
        }
    });
    
    // 设置心跳超时定时器
    connect(&m_heartbeatTimeoutTimer, &QTimer::timeout, this, &ChatManager::onHeartbeatTimeout);
    
    // 设置重连定时器
    connect(&m_reconnectTimer, &QTimer::timeout, this, &ChatManager::tryReconnect);
    
    // 处理心跳响应
    if (m_connection) {
        QObject::connect(m_connection.get(), &ChatConnection::heartbeatResponseReceived, this, [this]() {
            qDebug() << "ChatManager: 收到心跳响应";
            m_heartbeatTimeoutTimer.stop();
        });
    }
    
    // 启动心跳定时器
    m_heartbeatTimer.start(HEARTBEAT_INTERVAL);
}

void ChatManager::stopHeartbeatMonitor() {
    // 停止所有定时器
    m_heartbeatTimer.stop();
    m_heartbeatTimeoutTimer.stop();
    
    // 断开所有连接
    QObject::disconnect(&m_heartbeatTimer, nullptr, this, nullptr);
    QObject::disconnect(&m_heartbeatTimeoutTimer, nullptr, this, nullptr);
}

void ChatManager::onHeartbeatTimeout() {
    qDebug() << "ChatManager: 心跳超时";
    emit heartbeatTimeout();
    
    // 如果心跳超时，尝试重连
    if (m_autoReconnect) {
        if (m_connection) {
            m_connection->disconnect();
        }
    }
}

void ChatManager::tryReconnect() {
    m_reconnectAttempts++;
    qDebug() << "ChatManager: 尝试重连，第" << m_reconnectAttempts << "次";
    
    if (m_reconnectAttempts > MAX_RECONNECT_ATTEMPTS) {
        qDebug() << "ChatManager: 达到最大重连次数，放弃重连";
        m_reconnectTimer.stop();
        m_autoReconnect = false;
        return;
    }
    
    // 重新连接
    if (!m_lastHost.isEmpty() && m_lastPort > 0) {
        connectToServer(m_lastHost, m_lastPort);
        
        // 如果有认证信息，在连接成功后自动重新认证
        if (m_connection && m_lastUid > 0 && !m_lastToken.isEmpty()) {
            QObject::connect(m_connection.get(), &ChatConnection::connected, this, [this]() {
                authenticate(m_lastUid, m_lastToken);
            }, Qt::SingleShotConnection);  // 只执行一次
        }
    }
}
