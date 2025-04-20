//
// Created by wellwei on 25-4-18.
//

#ifndef UCHAT_TCPCLIENT_H
#define UCHAT_TCPCLIENT_H

#include <QObject>
#include <QAbstractSocket>
#include <QTcpSocket>
#include <QTimer>
#include "Singleton.h"

class TcpClient : public QObject {
Q_OBJECT
public:
    explicit TcpClient(QObject *parent = nullptr);

    ~TcpClient() override;

    // 连接到 ChatServer
    void connectToServer(const QString &host, quint16 port, const QString &token);

    void disconnectFromServer();

    // 发送消息
    void sendMessage(const QString &message);

    // 获取连接状态
    bool isConnected() const;

signals:
    void connected(); // 连接成功信号
    void disconnected(); // 断开连接信号
    void messageReceived(const QJsonObject &message); // 接收到消息信号
    void errorOccurred(const QString &error); // 错误发生信号

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onErrorOccurred(QAbstractSocket::SocketError socketError);
    void onHeartbeatTimeout();

private:
    QTcpSocket *m_socket;
    QString m_token;
    QTimer *m_heartbeatTimer;
    QArrayData m_buffer;
    bool m_isConnected;

    static const int HEARTBEAT_INTERVAL = 30000; // 心跳包间隔时间（毫秒）
};


#endif //UCHAT_TCPCLIENT_H
