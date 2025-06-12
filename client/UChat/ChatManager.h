#pragma once

#include "ChatConnection.h"
#include "Singleton.h"
#include <QObject>
#include <QTimer>
#include <memory>

class ChatManager : public QObject, public Singleton<ChatManager> {
    Q_OBJECT

public:
    ~ChatManager();

    /**
     * @brief 连接到聊天服务器
     * @param host 服务器主机名或IP地址
     * @param port 服务器端口
     * @return 连接请求是否成功发起（注意：不表示连接是否已建立，连接建立是异步的，通过connected信号通知）
     */
    bool connectToServer(const QString &host, quint16 port);
    
    void disconnect();
    
    /**
     * @brief 向服务器发送认证请求
     * @param uid 用户ID
     * @param token 认证令牌
     * @return 认证请求是否成功发起（注意：不表示认证是否已完成，认证结果是异步的，通过authenticated或authenticationFailed信号通知）
     */
    bool authenticate(uint64_t uid, const QString &token);

    // 发送消息
    void sendMessage(uint64_t toUid, const QString &content, const QString &msgType = "text");
    
    // 获取连接状态
    bool isConnected() const;
    bool isAuthenticated() const;

    // 心跳和重连控制
    void startHeartbeatMonitor();
    void stopHeartbeatMonitor();

signals:
    // 连接状态信号
    void connected();
    void disconnected();
    void authenticated();
    void authenticationFailed(int errorCode, const QString &errorMessage);
    
    // 消息信号
    void messageReceived(uint64_t fromUid, const QString &content, const QString &msgType, qint64 timestamp);
    void messageSent(uint64_t toUid, const QString &content, bool success, const QString &message);
    
    // 错误信号
    void error(const QString &errorMessage);
    
    // 心跳信号
    void heartbeatTimeout();

private slots:
    void onHeartbeatTimeout();
    void tryReconnect();

private:
    std::shared_ptr<ChatConnection> m_connection;
    
    // 心跳监控
    QTimer m_heartbeatTimer;
    QTimer m_heartbeatTimeoutTimer;
    QTimer m_reconnectTimer;
    
    // 重连相关
    QString m_lastHost;
    quint16 m_lastPort;
    uint64_t m_lastUid;
    QString m_lastToken;
    bool m_autoReconnect;
    int m_reconnectAttempts;
    
    // 常量
    static const int HEARTBEAT_INTERVAL = 10000;      // 10秒
    static const int HEARTBEAT_TIMEOUT = 30000;       // 30秒
    static const int RECONNECT_INTERVAL = 5000;       // 5秒
    static const int MAX_RECONNECT_ATTEMPTS = 5;      // 最大重连次数
};