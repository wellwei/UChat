#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QStandardItemModel>
#include <QListWidgetItem>
#include <QMap>
#include <QMenu>
#include <QAction>
#include <QSystemTrayIcon>
#include "ChatConnection.h"
#include "global.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

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

struct Contact {
    uint64_t uid;
    QString name;
    QString avatarPath;
    QString status;
    bool isOnline;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void showLoginDialog();
    void onLoginSuccess(const ServerInfo &serverInfo);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    // 导航按钮事件
    void onChatNavButtonClicked();
    void onContactsNavButtonClicked();
    void onGroupsNavButtonClicked();
    void onSettingsNavButtonClicked();

    // 聊天功能
    void onConversationSelected(QListWidgetItem *item);
    void onSendButtonClicked();
    void onMessageInputTextChanged();
    void onEmojiButtonClicked();
    void onFileButtonClicked();
    void onScreenshotButtonClicked();
    
    // 联系人功能
    void onContactSelected(const QModelIndex &index);
    void onAddContactClicked();
    void onContactContextMenu(const QPoint &pos);
    
    // 搜索功能
    void onSearchConversations(const QString &text);
    void onSearchContacts(const QString &text);
    void onSearchGroups(const QString &text);
    
    // 系统托盘功能
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayActionTriggered();
    
    // 网络事件
    void onConnectionStateChanged(ChatConnection::ConnectionState state);
    void onMessageReceived(uint64_t fromUid, const QString &content, const QString &msgType, qint64 timestamp);
    void onMessageSent(uint64_t toUid, const QString &content, bool success, const QString &message);
    void onNetworkError(const QString &errorMessage);

private:
    // UI初始化方法
    void initUI();
    void initTrayIcon();
    void initChatConnection();
    void initModels();
    void setupSignalsAndSlots();
    
    // 聊天相关方法
    void loadConversations();
    void updateConversationList();
    void createConversationItem(const Conversation &conversation);
    void loadChatHistory(uint64_t uid);
    void displayMessage(const ChatMessage &message);
    void addMessageToBrowser(const ChatMessage &message);
    void scrollToBottom();
    QString formatMessageTime(qint64 timestamp);
    
    // 联系人相关方法
    void loadContacts();
    void updateContactsModel();
    QStandardItem* createContactItem(const Contact &contact);
    
    // 帮助方法
    void startNewChat(uint64_t uid, const QString &name);
    QPixmap createAvatarPixmap(const QString &avatarPath, bool isOnline = true);
    void showNotification(const QString &title, const QString &message);

private:
    Ui::MainWindow *ui;
    
    // 模型
    QStandardItemModel *m_contactsModel;
    QMap<uint64_t, Conversation> m_conversations;
    QMap<uint64_t, Contact> m_contacts;
    
    // 当前状态
    uint64_t m_currentChatUid;
    uint64_t m_currentUserId;
    QString m_currentUserName;
    QString m_currentUserAvatar;
    
    // 系统托盘
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    
    // 网络连接
    ChatConnection *m_chatConnection;
    bool m_isConnected;
    ServerInfo m_serverInfo;
};
#endif // MAINWINDOW_HPP
