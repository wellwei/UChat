#include "ChatConnection.h"
#include <QDebug>
#include <QOverload>

ChatConnection::ChatConnection(QObject *parent)
    : QObject(parent),
      m_state(Disconnected),
      m_sending(false),
      m_nextMsgId(0) 
{
    m_headerBuffer.resize(HEADER_SIZE);

    // 连接信号
    connect(&m_socket, &QTcpSocket::connected, this, &ChatConnection::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &ChatConnection::onDisconnected);
    connect(&m_socket, &QTcpSocket::readyRead, this, &ChatConnection::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &ChatConnection::onError);

    // 设置心跳计时器
    m_heartbeatTimer.setInterval(HEARTBEAT_INTERVAL);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &ChatConnection::onHeartbeatTimeout);
}

ChatConnection::~ChatConnection() {
    disconnect();
}

void ChatConnection::connectToServer(const QString &host, quint16 port) {
    if (m_state != Disconnected) {
        qWarning() << "已连接到服务器，关闭当前连接重新连接";
        disconnect();
    }

    m_host = host;
    m_port = port;

    setState(Connecting);
    m_socket.connectToHost(host, port);
}

void ChatConnection::disconnect() {
    stopHeartbeat();

    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.disconnectFromHost();
        if (m_socket.state() != QAbstractSocket::UnconnectedState) {
            m_socket.abort();
        }
    }

    setState(Disconnected);
}

void ChatConnection::authenticate(uint64_t uid, const QString &token) {
    if (m_state != Connected) {
        qWarning() << "未连接到服务器，无法认证";
        emit error("Cannot authenticate: not connected");
        return;
    }

    m_uid = uid;
    m_token = token;

    QJsonObject data;
    data["uid"] = QString::number(uid);
    data["token"] = token;

    QJsonObject message;
    message["type"] = "auth_req";
    message["data"] = data;

    setState(Authenticating);
    sendJsonMessage(message, m_nextMsgId++);
}

void ChatConnection::sendChatMessage(uint64_t toUid, const QString &content, const QString &msgType) {
    if (!isAuthenticated()) {
        qWarning() << "未认证，无法发送消息";
        emit error("Cannot send message: not authenticated");
        return;
    }

    QJsonObject data;
    data["to_uid"] = QString::number(toUid);
    data["content"] = content;
    data["msg_type"] = msgType;

    QJsonObject message;
    message["type"] = "send_msg_req";
    message["data"] = data;

    sendJsonMessage(message, m_nextMsgId++);
}

void ChatConnection::sendHeartbeat() {
    if (!isConnected()) {
        qWarning() << "未连接到服务器，无法发送心跳";
        return;
    }

    QJsonObject message;
    message["type"] = "heartbeat_req";
    message["data"] = QJsonObject();

    sendJsonMessage(message, m_nextMsgId++);
}

void ChatConnection::onConnected() {
    qDebug() << "连接成功";
    setState(Connected);
    emit connected();
}

void ChatConnection::onDisconnected() {
    qDebug() << "连接断开";
    stopHeartbeat();
    setState(Disconnected);
    emit disconnected();
}

void ChatConnection::onReadyRead() {
    if (m_socket.bytesAvailable() >= HEADER_SIZE) {
        readHeader();
    }
}

void ChatConnection::readHeader() {
    if (m_socket.read(m_headerBuffer.data(), HEADER_SIZE) != HEADER_SIZE) {
        qWarning() << "读取头部失败";
        emit error("Failed to read header");
        return;
    }

    quint16 msgId = ((static_cast<quint8>(m_headerBuffer[0]) << 8) |
                      static_cast<quint8>(m_headerBuffer[1]));
    quint16 msgLen = ((static_cast<quint8>(m_headerBuffer[2]) << 8) |
                      static_cast<quint8>(m_headerBuffer[3]));

    if (msgLen > 0 && msgLen <= MAX_CONTENT_LENGTH) {
        readBody(msgId, msgLen);
    } else {
        qWarning() << "消息长度超出限制" << msgLen;
        emit error("Invalid message length");
    }
}

void ChatConnection::readBody(quint16 msgId, quint16 msgLen) {
    m_bodyBuffer.resize(msgLen);

    if (m_socket.bytesAvailable() >= msgLen) {
        if (m_socket.read(m_bodyBuffer.data(), msgLen) != msgLen) {
            qWarning() << "读取消息体失败";
            emit error("Failed to read message body");
            return;
        }
        processMessage(msgId, m_bodyBuffer);
    } else {
        qWarning() << "消息体不完整, 等待更多数据" << msgLen << m_socket.bytesAvailable();

        qint64 bytesNeeded = msgLen;
        qint64 bytesRead = 0;

        while (bytesRead < bytesNeeded) {
            if (!m_socket.waitForReadyRead(5000)) {
                qWarning() << "等待消息体超时";
                emit error("Message body read timeout");
                return;
            }

            qint64 bytes = m_socket.read(m_bodyBuffer.data() + bytesRead, bytesNeeded - bytesRead);
            if (bytes < 0) {
                qWarning() << "读取消息体失败";
                emit error("Failed to read message body");
                return;
            }

            bytesRead += bytes;
        }
        
        processMessage(msgId, m_bodyBuffer);
    }
        
    // 继续读取下一个消息
    if (m_socket.bytesAvailable() >= HEADER_SIZE) {
        readHeader();
    }    
}

void ChatConnection::processMessage(quint16 msgId, const QByteArray &data) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "解析消息失败" << data;
        emit error("Failed to parse message");
        return;
    }

    QJsonObject message = doc.object();
    if (!message.contains("type") || !message["type"].isString()) {
        qWarning() << "消息格式错误" << data;
        emit error("Invalid message format: missing or invalid type");
        return;
    }
    
    QString type = message["type"].toString();

    if (type == "auth_resp") {
        int code = message["code"].toInt();
        if (code == 0) {
            qDebug() << "认证成功";
            setState(Authenticated);
            emit authenticated();
            startHeartbeat();
        } else {
            qWarning() << "认证失败" << code;
            QString errorMsg = message["message"].toString();
            emit authenticationFailed(code, errorMsg);
            setState(Connected);
        }
    } else if (type == "send_msg_resp") {
        int code = message.value("code").toInt();
        
    }
}

void ChatConnection::sendJsonMessage(const QJsonObject &message, quint16 msgId)
{
    QJsonDocument doc(message);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    
    // 构建消息头部（消息ID和内容长度）
    QByteArray header;
    QDataStream headerStream(&header, QIODevice::WriteOnly);
    headerStream.setByteOrder(QDataStream::LittleEndian);
    headerStream << msgId;
    headerStream << static_cast<quint16>(jsonData.size());
    
    // 发送数据
    m_socket.write(header);
    m_socket.write(jsonData);
}

void ChatConnection::setState(ConnectionState newState)
{
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(m_state);
        
        switch (m_state) {
            case Connected:
                emit connected();
                break;
            case Disconnected:
                emit disconnected();
                break;
            case Authenticated:
                emit authenticated();
                break;
            default:
                break;
        }
    }
}

void ChatConnection::startHeartbeat()
{
    // 启动心跳定时器，默认30秒发送一次
    m_heartbeatTimer.start(HEARTBEAT_INTERVAL);
}

void ChatConnection::stopHeartbeat()
{
    // 停止心跳定时器
    m_heartbeatTimer.stop();
}

void ChatConnection::onHeartbeatTimeout()
{
    if (isConnected()) {
        sendHeartbeat();
    }
}

void ChatConnection::processPendingMessages()
{
    QMutexLocker locker(&m_queueMutex);
    
    if (m_messageQueue.isEmpty() || m_sending) {
        return;
    }
    
    m_sending = true;
    
    PendingMessage msg = m_messageQueue.dequeue();
    sendJsonMessage(msg.content, msg.msgId);
    
    m_sending = false;
    
    // 如果队列中还有消息，继续处理
    if (!m_messageQueue.isEmpty()) {
        QMetaObject::invokeMethod(this, "processPendingMessages", Qt::QueuedConnection);
    }
}

void ChatConnection::onError(QAbstractSocket::SocketError socketError)
{
    // 处理网络错误
    QString errorMessage = m_socket.errorString();
    qDebug() << "Socket error:" << socketError << errorMessage;
    
    // 设置连接状态为错误
    setState(Error);
    
    // 发送错误信号
    emit error(errorMessage);
    
    // 停止心跳
    stopHeartbeat();
}
      