#ifndef CONTACTMANAGER_H
#define CONTACTMANAGER_H

#include <QObject>
#include <QMap>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QJsonObject>
#include <QJsonArray>
#include "global.h"
#include "Singleton.h"

struct Contact {
    uint64_t uid;
    QString name;
    QString avatar_url;
    QString status;
    bool isOnline;
    QString signature;
    QString avatarPath;
};

class ContactManager : public QObject, public Singleton<ContactManager>
{
    Q_OBJECT

public:
    ~ContactManager();

    // 获取联系人列表
    void getContacts(const uint64_t &uid, const QString &token);
    
    // 添加联系人
    void addContact(const uint64_t &uid, const QString &token, const uint64_t &friendId);
    
    // 搜索用户
    void searchUser(const uint64_t &uid, const QString &token, const QString &keyword);
    
    // 新增：联系人请求相关方法
    void sendContactRequest(const uint64_t &uid, const QString &token, const uint64_t &addresseeId, const QString &message);
    void handleContactRequest(const uint64_t &uid, const QString &token, const uint64_t &requestId, ContactRequestStatus action);
    void getContactRequests(const uint64_t &uid, const QString &token, ContactRequestStatus status = PENDING);
    
    // 获取联系人信息
    bool getContactInfo(uint64_t uid, Contact &contact) const;
    
    // 获取所有联系人
    const QMap<uint64_t, Contact>& getAllContacts() const;
    
    // 获取联系人请求列表
    const QList<ContactRequest>& getContactRequestsList() const;
    
    // 创建联系人项目
    QStandardItem* createContactItem(const Contact &contact);
    
    // 处理网络响应
    void processGetContactsResult(const QJsonObject &jsonObj);
    void processAddContactResult(const QJsonObject &jsonObj);
    void processSearchUserResult(const QJsonObject &jsonObj);
    void processSendContactRequestResult(const QJsonObject &jsonObj);
    void processHandleContactRequestResult(const QJsonObject &jsonObj);
    void processGetContactRequestsResult(const QJsonObject &jsonObj);
    
    // 创建头像
    static QPixmap createAvatarPixmap(const QString &avatarUrl, bool isOnline = true);

signals:
    void contactsUpdated();
    void contactAdded(uint64_t contactId);
    void userSearchCompleted(const QJsonArray &users);
    
    // 新增：联系人请求相关信号
    void contactRequestSent(uint64_t requestId);
    void contactRequestHandled(uint64_t requestId, uint64_t action);
    void contactRequestsUpdated();
    void newContactRequestReceived(const ContactRequest &request);

private:
    friend class Singleton<ContactManager>;
    ContactManager(QObject *parent = nullptr);
    
    QMap<uint64_t, Contact> m_contacts;
    QList<ContactRequest> m_contactRequests;
};

#endif // CONTACTMANAGER_H 