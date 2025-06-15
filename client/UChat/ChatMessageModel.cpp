#include "ChatMessageModel.h"

ChatMessageModel::ChatMessageModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ChatMessageModel::rowCount(const QModelIndex &parent) const
{
    // For list models, the parent is always invalid.
    if (parent.isValid())
        return 0;
    return m_messages.count();
}

QVariant ChatMessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_messages.count())
        return QVariant();

    const ChatMessage &message = m_messages.at(index.row());
    switch (static_cast<ChatMessage::Role>(role)) {
        case ChatMessage::SenderIdRole:
            return QVariant::fromValue(message.senderId);
        case ChatMessage::ReceiverIdRole:
            return QVariant::fromValue(message.receiverId);
        case ChatMessage::ContentRole:
            return message.content;
        case ChatMessage::TypeRole:
            return message.type;
        case ChatMessage::TimestampRole:
            return message.timestamp;
        case ChatMessage::IsOutgoingRole:
            return message.isOutgoing;
        case ChatMessage::AvatarPathRole:
            return message.avatarPath;
    }

    return QVariant();
}

QHash<int, QByteArray> ChatMessageModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ChatMessage::SenderIdRole] = "senderId";
    roles[ChatMessage::ReceiverIdRole] = "receiverId";
    roles[ChatMessage::ContentRole] = "content";
    roles[ChatMessage::TypeRole] = "type";
    roles[ChatMessage::TimestampRole] = "timestamp";
    roles[ChatMessage::IsOutgoingRole] = "isOutgoing";
    roles[ChatMessage::AvatarPathRole] = "avatarPath";
    return roles;
}


void ChatMessageModel::addMessage(const ChatMessage &message)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    m_messages.append(message);
    endInsertRows();
}

void ChatMessageModel::clearMessages()
{
    if(m_messages.isEmpty()){
        return;
    }
    beginResetModel();
    m_messages.clear();
    endResetModel();
} 