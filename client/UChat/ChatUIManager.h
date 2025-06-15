#ifndef CHATUIMANAGER_H
#define CHATUIMANAGER_H

#include <QObject>
#include <QMap>
#include "global.h"
#include "Singleton.h"
#include "ChatMessageModel.h"

// Conversation 结构体
struct Conversation {
    uint64_t uid;
    QString name;
    QString avatarPath;
    QString lastMessage;
    qint64 lastMessageTime;
    int unreadCount;
};

class QListView;
class QListWidget;
class QListWidgetItem;
class QLabel;

class ChatUIManager : public QObject, public Singleton<ChatUIManager>
{
    Q_OBJECT

public:
    ~ChatUIManager();

    // 初始化UI控件引用
    void initUI(QListWidget *conversationsList, QListView *messageListView, QLabel *chatTitleLabel);
    
    // 加载会话列表
    void loadConversations(const QMap<uint64_t, Conversation> &conversations);
    
    // 更新会话列表
    void updateConversationList();
    
    // 创建会话项
    void createConversationItem(const Conversation &conversation);
    
    // 加载聊天历史
    void loadChatHistory(uint64_t uid);
    
    // 添加新消息
    void addMessage(uint64_t chatUid, const ChatMessage &message);
    
    // 获取当前聊天对象ID
    uint64_t getCurrentChatUid() const;
    
    // 设置当前聊天对象ID
    void setCurrentChatUid(uint64_t uid);
    
    // 设置当前用户信息
    void setCurrentUserInfo(uint64_t userId, const QString &userName);
    
    // 获取会话
    QMap<uint64_t, Conversation>& getConversations();
    
    // 开始新会话
    void startNewChat(uint64_t uid, const QString &name, const QString &avatarPath = QString());

    // 创建或获取会话（新增方法）
    void createOrGetConversation(uint64_t uid, const QString &name = QString(), const QString &avatarPath = QString());

    // 未读消息管理方法
    int getTotalUnreadCount() const;
    int getUnreadCount(uint64_t uid) const;
    void markAsRead(uint64_t uid);
    void markAllAsRead();

signals:
    void conversationSelected(uint64_t uid);
    
    // 未读消息相关信号
    void unreadCountChanged(uint64_t uid, int count);
    void totalUnreadCountChanged(int totalCount);
    void newMessageReceived(uint64_t fromUid, const QString &content);

private slots:
    void onConversationSelected(QListWidgetItem *item);

private:
    friend class Singleton<ChatUIManager>;

    explicit ChatUIManager(QObject *parent = nullptr);
    
    // 滚动到底部
    void scrollToBottom();
    
    // 格式化消息时间
    static QString formatMessageTime(qint64 timestamp);
    
    QListWidget *m_conversationsList;
    QListView *m_messageListView;
    QLabel *m_chatTitleLabel;
    
    ChatMessageModel *m_chatModel;

    QMap<uint64_t, Conversation> m_conversations;   // uid -> 会话项
    QMap<uint64_t, QList<ChatMessage>> m_messageHistories;
    uint64_t m_currentChatUid;
    uint64_t m_currentUserId;
    QString m_currentUserName;
};

#endif // CHATUIMANAGER_H 