#include "ChatConnection.h"
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QThread>

ChatConnection::ChatConnection(QObject *parent)
    : QObject(parent),
      m_state(Disconnected),
      m_sending(false),
      m_nextMsgId(0),
      m_lastActiveTime(QDateTime::currentSecsSinceEpoch())
{
    m_headerBuffer.resize(HEADER_SIZE);

    // 连接信号
    connect(&m_socket, &QTcpSocket::connected, this, &ChatConnection::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &ChatConnection::onDisconnected);
    connect(&m_socket, &QTcpSocket::readyRead, this, &ChatConnection::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &ChatConnection::onError);
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
    
    try {
    m_socket.connectToHost(host, port);
        qDebug() << "发起连接请求：" << host << ":" << port;
    } catch (const std::exception &e) {
        qWarning() << "连接失败：" << e.what();
        setState(Error);
        emit error(QString("连接失败: %1").arg(e.what()));
        throw; // 重新抛出异常以便上层处理
    }
}

void ChatConnection::disconnect() {

    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.disconnectFromHost();
        if (m_socket.state() != QAbstractSocket::UnconnectedState) {
            m_socket.abort();
        }
    }

    setState(Disconnected);
}

void ChatConnection::authenticate(uint64_t uid, const QString &token) {
    if (m_state != Connected && m_state != Authenticating) {
        QString errMsg = QString("认证失败: 当前连接状态不是Connected，而是 %1").arg(m_state);
        qWarning() << errMsg;
        emit error("无法认证: 连接未就绪");
        throw std::runtime_error("无法认证: 连接未就绪");
    }

    m_uid = uid;
    m_token = token;

    try {
        // 构建认证请求
        QJsonObject data;
        data["uid"] = QString::number(uid);
        data["token"] = token;

        QJsonObject message;
        message["type"] = "auth_req";
        message["data"] = data;

        setState(Authenticating);
        
        QString logMsg = QString("===== 发送认证请求 ===== UID: %1, Token: %2...")
                        .arg(uid)
                        .arg(token.left(10));
        qDebug() << logMsg;
        
        // 记录套接字状态
        QString socketState = QString("套接字状态: 已连接=%1, 开启状态=%2, 错误=%3, 状态=%4")
                             .arg(m_socket.isValid())
                             .arg(m_socket.isOpen())
                             .arg(m_socket.error())
                             .arg(m_socket.state());
        qDebug() << socketState;

        sendJsonMessage(message, m_nextMsgId++);
    } catch (const std::exception &e) {
        QString errMsg = QString("发送认证请求失败: %1").arg(e.what());
        qCritical() << errMsg;
        setState(Connected);  // 恢复到已连接状态
        emit error(errMsg);
        throw; // 重新抛出异常以便上层处理
    }
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
}

void ChatConnection::onDisconnected() {
    qDebug() << "连接断开";
    setState(Disconnected);
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

    // 使用网络字节序（大端序）解析消息头
    quint16 msgId = ((static_cast<quint8>(m_headerBuffer[0]) << 8) |
                      static_cast<quint8>(m_headerBuffer[1]));
    quint16 msgLen = ((static_cast<quint8>(m_headerBuffer[2]) << 8) |
                      static_cast<quint8>(m_headerBuffer[3]));

    QString logMsg = QString("收到消息头: ID=%1, 长度=%2").arg(msgId).arg(msgLen);
    qDebug() << logMsg;

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
            qInfo() << "认证成功 (UID:" << m_uid << ")";
            setState(Authenticated);
        } else {
            QString errorMsg = message["message"].toString();
            qWarning() << "认证失败: 代码=" << code << ", 原因=" << errorMsg << " (UID:" << m_uid << ")";
            emit authenticationFailed(code, errorMsg);
            setState(Connected);
        }
    } else if (type == "send_msg_resp") {
        int code = message.value("code").toInt();
        bool success = (code == 0);
        QString msg = message.value("message").toString();
        
        if (message.contains("data") && message["data"].isObject()) {
            QJsonObject data = message["data"].toObject();
            if (data.contains("to_uid") && data.contains("content")) {
                uint64_t toUid = data["to_uid"].toString().toULongLong();
                QString content = data["content"].toString();
                emit messageSent(toUid, content, success, msg);
            }
        }
    } else if (type == "recv_msg") {
        if (message.contains("data") && message["data"].isObject()) {
            QJsonObject data = message["data"].toObject();
            if (data.contains("from_uid") && data.contains("content") && data.contains("msg_type")) {
                uint64_t fromUid = data["from_uid"].toString().toULongLong();
                QString content = data["content"].toString();
                QString msgType = data["msg_type"].toString();
                qint64 timestamp = data.contains("timestamp") ? data["timestamp"].toVariant().toLongLong() : QDateTime::currentSecsSinceEpoch();
                
                emit messageReceived(fromUid, content, msgType, timestamp);
            }
        }
    } else if (type == "heartbeat_resp") {
        // 处理心跳响应
        qDebug() << "收到心跳响应";
        // 可以在这里重置计时器或更新最后活跃时间
        updateLastActiveTime();
        // 不要将心跳响应作为消息发送到UI
        // emit messageReceived(0, "", "heartbeat_resp", QDateTime::currentSecsSinceEpoch());
        
        // 可以添加专门的心跳响应信号
        emit heartbeatResponseReceived();
    } else if (type == "error_resp") {
        int code = message.value("code").toInt();
        QString msg = message.value("message").toString();
        emit error(msg);
    }
    else {
        qWarning() << "未知消息类型" << type;
    }
}

void ChatConnection::sendJsonMessage(const QJsonObject &message, quint16 msgId)
{
    QJsonDocument doc(message);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    
    // 构建消息头部（消息ID和内容长度）
    QByteArray header;
    header.resize(HEADER_SIZE);
    
    // 使用网络字节序（大端序）
    header[0] = (msgId >> 8) & 0xFF;
    header[1] = msgId & 0xFF;
    header[2] = (jsonData.size() >> 8) & 0xFF;
    header[3] = jsonData.size() & 0xFF;
    
    QString logMsg = QString("发送消息: ID=%1, 类型=%2, 长度=%3, 内容=%4")
                    .arg(msgId)
                    .arg(message["type"].toString())
                    .arg(jsonData.size())
                    .arg(QString(jsonData));
    
    qDebug() << logMsg;
             
    // 发送数据
    qint64 headerBytes = m_socket.write(header);
    qint64 dataBytes = m_socket.write(jsonData);
    
    if (headerBytes != HEADER_SIZE || dataBytes != jsonData.size()) {
        QString errMsg = QString("消息发送不完整: 头部=%1/%2, 数据=%3/%4")
                        .arg(headerBytes).arg(HEADER_SIZE)
                        .arg(dataBytes).arg(jsonData.size());
        qWarning() << errMsg;
    }
    
    // 强制立即发送
    m_socket.flush();
}

void ChatConnection::setState(ConnectionState newState)
{
    if (m_state != newState) {
        qDebug() << "连接状态变更: " << m_state << " -> " << newState;
        m_state = newState;
        
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
}
      
void ChatConnection::updateLastActiveTime()
{
    m_lastActiveTime = QDateTime::currentSecsSinceEpoch();
}