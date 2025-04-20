//
// Created by wellwei on 25-4-18.
//

#include "TcpClient.h"
#include <QJsonDocument>

TcpClient::TcpClient(QObject *parent)
        : QObject(parent),
          m_socket(new QTcpSocket(this)),
          m_heartbeatTimer(new QTimer(this)),
          m_isConnected(false) {
    connect(m_socket, &QTcpSocket::connected, this, &TcpClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpClient::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &TcpClient::onErrorOccurred);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &TcpClient::onHeartbeatTimeout);

    m_heartbeatTimer->setInterval(HEARTBEAT_INTERVAL);

}