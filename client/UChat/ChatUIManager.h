#ifndef CHATUIMANAGER_H
#define CHATUIMANAGER_H

#include <QObject>
#include <QMap>
#include <QListWidgetItem>
#include "global.h"
#include "Singleton.h"

struct ChatMessage {
    uint64_t senderId;
    uint64_t receiverId;
    QString content;
    QString type;
    qint64 timestamp;
    bool isOutgoing;
};

struct Conversation {
    uint64_t uid;
    QString name;
    QString avatarPath;
    QList<ChatMessage> messages;
    QString lastMessage;
    qint64 lastMessageTime;
    int unreadCount;
};

class QTextBrowser;
class QListWidget;
class QLabel;

class ChatUIManager : public QObject, public Singleton<ChatUIManager>
{
    Q_OBJECT

public:
    ~ChatUIManager();

    // 初始化UI控件引用
    void initUI(QListWidget *conversationsList, QTextBrowser *messageBrowser, QLabel *chatTitleLabel);
    
    // 加载会话列表
    void loadConversations(const QMap<uint64_t, Conversation> &conversations);
    
    // 更新会话列表
    void updateConversationList();
    
    // 创建会话项
    void createConversationItem(const Conversation &conversation);
    
    // 加载聊天历史
    void loadChatHistory(uint64_t uid);
    
    // 显示消息
    void displayMessage(const ChatMessage &message);
    
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

signals:
    void conversationSelected(uint64_t uid);
    void messageDisplayed();

private slots:
    void onConversationSelected(QListWidgetItem *item);

private:
    friend class Singleton<ChatUIManager>;
    ChatUIManager(QObject *parent = nullptr);
    
    // 添加消息到浏览器
    void addMessageToBrowser(const ChatMessage &message);
    
    // 滚动到底部
    void scrollToBottom();
    
    // 格式化消息时间
    static QString formatMessageTime(qint64 timestamp);
    
    QListWidget *m_conversationsList;
    QTextBrowser *m_messageBrowser;
    QLabel *m_chatTitleLabel;
    
    QMap<uint64_t, Conversation> m_conversations;
    uint64_t m_currentChatUid;
    uint64_t m_currentUserId;
    QString m_currentUserName;
};

#endif // CHATUIMANAGER_H 