#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QByteArray>
#include <QQueue>
#include <QMutex>
#include <memory>

class ChatConnection : public QObject {
    Q_OBJECT
public:
    enum ConnectionState {
        Disconnected,
        Connecting,
        Connected,
        Authenticating,
        Authenticated,
        Error
    };

    explicit ChatConnection(QObject *parent = nullptr);
    ~ChatConnection() override;

    void connectToServer(const QString &host, quint16 port);
    void disconnect();

    bool isConnected() const { return m_state == Connected || m_state == Authenticated || m_state == Authenticating; }
    bool isAuthenticated() const { return m_state == Authenticated; }
    ConnectionState state() const { return m_state; }

    void authenticate(uint64_t uid, const QString &token);
    void sendChatMessage(uint64_t targetUid, const QString &content, const QString &msgType = "text");
    void sendHeartbeat();

signals:
    void stateChanged(ConnectionState newState);
    void connected();
    void disconnected();
    void authenticated();
    void authenticationFailed(int errorCode, const QString &errorMessage);
    void messageReceived(uint64_t fromUid, const QString &content, const QString &msgType, qint64 timestamp);
    void messageSent(uint64_t toUid, const QString &content, bool success, const QString &message);
    void error(const QString &errorMessage);
    void heartbeatResponseReceived();

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError socketError);
    void processPendingMessages();

private:
    void setState(ConnectionState newState);
    void sendJsonMessage(const QJsonObject &message, quint16 msgId);
    void readHeader();
    void readBody(quint16 msgId, quint16 contentLength);

    void updateLastActiveTime();

    void processMessage(quint16 msgId, const QByteArray &data);

private:
    QTcpSocket m_socket;
    ConnectionState m_state;
    uint64_t m_uid{};
    QString m_token;
    QString m_host;
    quint16 m_port{};
    qint64 m_lastActiveTime;
    
    // 消息缓冲区
    QByteArray m_headerBuffer;
    QByteArray m_bodyBuffer;

    // 待发送消息队列
    struct PendingMessage {
        quint16 msgId;
        QJsonObject content;
    };

    QQueue<PendingMessage> m_messageQueue;
    QMutex m_queueMutex;
    bool m_sending;

    // 消息ID计数器
    quint16 m_nextMsgId;

    // 常量
    static const quint16 HEADER_SIZE = 4; // 消息头大小
    static const quint16 MAX_CONTENT_LENGTH = 65535; // 最大内容长度
};
