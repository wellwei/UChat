#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include "global.h"

// 前向声明
class QStandardItemModel;
class QListWidgetItem;
class QModelIndex;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void showLoginDialog();
    void onLoginSuccess(const ClientInfo &clientInfo);

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
    void onSendButtonClicked();
    void onMessageInputTextChanged();
    void onEmojiButtonClicked();
    void onFileButtonClicked();
    void onScreenshotButtonClicked();
    
    // 联系人功能
    void onContactSelected(const QModelIndex &index);
    void onAddContactClicked();
    void onContactContextMenu(const QPoint &pos);
    void updateContactsView();
    void displaySearchUserResults(const QJsonArray &users);

    
    // 搜索功能
    void onSearchConversations(const QString &text);
    void onSearchContacts(const QString &text);
    void onSearchGroups(const QString &text);
    
    // 系统托盘功能
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayActionTriggered();
    
    // 网络事件
    void onMessageReceived(uint64_t fromUid, const QString &content, const QString &msgType, qint64 timestamp);
    void onMessageSent(uint64_t toUid, const QString &content, bool success, const QString &message);
    void onNetworkError(const QString &errorMessage);

private:
    // UI初始化方法
    void initUI();
    void initTrayIcon();
    void setupSignalsAndSlots();
    
    // 帮助方法
    void showNotification(const QString &title, const QString &message);

    // 更新连接状态UI
    void updateConnectionStatus(bool isConnected, const QString &statusMessage = QString());

private:
    Ui::MainWindow *ui;
    
    // 联系人模型
    QStandardItemModel *m_contactsModel;
    
    // 系统托盘
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    
    // 网络连接
    bool m_isConnected;
    ClientInfo m_clientInfo;
};
#endif // MAINWINDOW_HPP
