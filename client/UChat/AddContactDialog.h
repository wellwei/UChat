#ifndef ADDCONTACTDIALOG_H
#define ADDCONTACTDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTabWidget>
#include <QScrollArea>
#include <QTextEdit>
#include <QGroupBox>
#include <QFrame>
#include "global.h"

class SearchResultItem;

class AddContactDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddContactDialog(QWidget *parent = nullptr);
    ~AddContactDialog();

    // 设置当前用户信息
    void setCurrentUser(uint64_t uid, const QString &token);
    
    // 刷新所有数据
    void refreshData();

signals:
    void contactRequestAccepted(); // 联系人请求被接受时发出信号

private slots:
    void onSearchButtonClicked();
    void onSearchTextChanged();
    void onRefreshRequestsClicked();
    void onUserSearchCompleted(const QJsonArray &users);
    void onContactRequestsUpdated();
    void onContactRequestSent(uint64_t requestId);
    void onContactRequestHandled(uint64_t requestId, uint64_t action);
    void onSendRequestToUser(uint64_t targetUid, const QString &targetName);
    void onAcceptRequest(uint64_t requestId);
    void onRejectRequest(uint64_t requestId);

private:
    void setupUI();
    void setupConnections();
    void clearSearchResults();
    void updateRequestsList();
    void setupSearchPage();
    void setupRequestsPage();
    QWidget* createUserResultItem(const QJsonObject &user);
    QWidget* createRequestItem(const ContactRequest &request);

private:
    QTabWidget *m_tabWidget;
    
    // 搜索用户页面
    QWidget *m_searchPage;
    QLineEdit *m_searchLineEdit;
    QPushButton *m_searchButton;
    QScrollArea *m_searchResultsArea;
    QWidget *m_searchResultsWidget;
    QVBoxLayout *m_searchResultsLayout;
    QLabel *m_searchStatusLabel;
    
    // 联系人请求页面
    QWidget *m_requestsPage;
    QPushButton *m_refreshRequestsButton;
    QScrollArea *m_requestsScrollArea;
    QWidget *m_requestsListWidget;
    QVBoxLayout *m_requestsListLayout;
    QLabel *m_requestsStatusLabel;
    
    // 当前用户信息
    uint64_t m_currentUid;
    QString m_currentToken;
};

// 搜索结果项目Widget
class SearchResultItem : public QFrame
{
    Q_OBJECT
    
public:
    explicit SearchResultItem(const QJsonObject &userData, QWidget *parent = nullptr);
    
    uint64_t getUserId() const { return m_userId; }
    QString getUserName() const { return m_userName; }

signals:
    void sendRequestClicked(uint64_t userId, const QString &userName);

private slots:
    void onSendRequestClicked();

private:
    void setupUI();

private:
    uint64_t m_userId;
    QString m_userName;
    QString m_userEmail;
    
    QLabel *m_avatarLabel;
    QLabel *m_nameLabel;
    QLabel *m_emailLabel;
    QPushButton *m_sendRequestButton;
};

// 联系人请求项目Widget  
class ContactRequestItem : public QFrame
{
    Q_OBJECT
    
public:
    explicit ContactRequestItem(const ContactRequest &request, QWidget *parent = nullptr);
    
    uint64_t getRequestId() const { return m_requestId; }

signals:
    void acceptClicked(uint64_t requestId);
    void rejectClicked(uint64_t requestId);

private slots:
    void onAcceptClicked();
    void onRejectClicked();

private:
    void setupUI();

private:
    uint64_t m_requestId;
    uint64_t m_requesterId;
    QString m_requesterName;
    QString m_requestMessage;
    ContactRequestStatus m_status;
    
    QLabel *m_avatarLabel;
    QLabel *m_nameLabel;
    QLabel *m_messageLabel;
    QLabel *m_statusLabel;
    QPushButton *m_acceptButton;
    QPushButton *m_rejectButton;
};

#endif // ADDCONTACTDIALOG_H 