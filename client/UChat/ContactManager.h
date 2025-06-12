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
    QString avatarPath;
    QString status;
    bool isOnline;
    QString signature;
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
    
    // 获取联系人信息
    bool getContactInfo(uint64_t uid, Contact &contact) const;
    
    // 获取所有联系人
    const QMap<uint64_t, Contact>& getAllContacts() const;
    
    // 创建联系人项目
    QStandardItem* createContactItem(const Contact &contact);
    
    // 处理网络响应
    void processGetContactsResult(const QJsonObject &jsonObj);
    void processAddContactResult(const QJsonObject &jsonObj);
    void processSearchUserResult(const QJsonObject &jsonObj);
    
    // 创建头像
    static QPixmap createAvatarPixmap(const QString &avatarPath, bool isOnline = true);

signals:
    void contactsUpdated();
    void contactAdded(uint64_t contactId);
    void userSearchCompleted(const QJsonArray &users);

private:
    friend class Singleton<ContactManager>;
    ContactManager(QObject *parent = nullptr);
    
    QMap<uint64_t, Contact> m_contacts;
};

#endif // CONTACTMANAGER_H 