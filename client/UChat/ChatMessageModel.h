#ifndef CHATMESSAGEMODEL_H
#define CHATMESSAGEMODEL_H

#include <QAbstractListModel>
#include <QList>
#include "global.h"

// 将 ChatMessage 结构体移到这里，因为它与模型紧密相关
struct ChatMessage {
    enum Role {
        SenderIdRole = Qt::UserRole + 1,
        ReceiverIdRole,
        ContentRole,
        TypeRole,
        TimestampRole,
        IsOutgoingRole,
        AvatarPathRole // 新增头像路径角色
    };

    uint64_t senderId;
    uint64_t receiverId;
    QString content;
    QString type;
    qint64 timestamp;
    bool isOutgoing;
    QString avatarPath; // 好友的头像路径
};

class ChatMessageModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit ChatMessageModel(QObject *parent = nullptr);

    // QAbstractItemModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Public API
    void addMessage(const ChatMessage &message);
    void clearMessages();
    
private:
    QList<ChatMessage> m_messages;
};

#endif // CHATMESSAGEMODEL_H 