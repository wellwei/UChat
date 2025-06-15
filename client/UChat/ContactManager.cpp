#include "ContactManager.h"

#include <QFile>

#include "HttpMgr.h"
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QHBoxLayout>
#include <qjsondocument.h>
#include <QVBoxLayout>
#include <QLabel>

ContactManager::ContactManager(QObject *parent) : QObject(parent)
{
    // 连接HTTP响应信号
    auto httpMgr = HttpMgr::getInstance();
    connect(httpMgr.get(), &HttpMgr::sig_chat_mod_finish, this, [this](ReqId id, const QString &res, ErrorCodes err) {
        if (err != ErrorCodes::SUCCESS) {
            qWarning() << "ContactManager: HTTP请求错误: " << getErrorMessage(err);
            return;
        }
        
        // 解析JSON响应
        QJsonParseError jsonError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8(), &jsonError);
        if (jsonError.error != QJsonParseError::NoError) {
            qWarning() << "ContactManager: JSON解析错误: " << jsonError.errorString();
            return;
        }
        
        QJsonObject jsonObj = jsonDoc.object();
        if (jsonObj["error"].toInt() != ErrorCodes::SUCCESS) {
            qWarning() << "ContactManager: 请求失败: " << jsonObj["message"].toString();
            return;
        }
        
        // 根据请求ID处理不同的响应
        switch (id) {
            case ReqId::ID_GET_CONTACTS:
                processGetContactsResult(jsonObj);
                break;
            case ReqId::ID_ADD_CONTACT:
                processAddContactResult(jsonObj);
                break;
            case ReqId::ID_SEARCH_USER:
                processSearchUserResult(jsonObj);
                break;
            case ReqId::ID_SEND_CONTACT_REQUEST:
                processSendContactRequestResult(jsonObj);
                break;
            case ReqId::ID_HANDLE_CONTACT_REQUEST:
                processHandleContactRequestResult(jsonObj);
                break;
            case ReqId::ID_GET_CONTACT_REQUESTS:
                processGetContactRequestsResult(jsonObj);
                break;
            default:
                break;
        }
    });
}

ContactManager::~ContactManager()
{
}

void ContactManager::getContacts(const uint64_t &uid, const QString &token)
{
    auto httpMgr = HttpMgr::getInstance();
    httpMgr->getContacts(uid, token);
}

void ContactManager::searchUser(const uint64_t &uid, const QString &token, const QString &keyword)
{
    auto httpMgr = HttpMgr::getInstance();
    httpMgr->searchUser(uid, token, keyword);
}

void ContactManager::sendContactRequest(const uint64_t &uid, const QString &token, const uint64_t &addresseeId, const QString &message)
{
    auto httpMgr = HttpMgr::getInstance();
    httpMgr->sendContactRequest(uid, token, addresseeId, message);
}

void ContactManager::handleContactRequest(const uint64_t &uid, const QString &token, const uint64_t &requestId, ContactRequestStatus action)
{
    auto httpMgr = HttpMgr::getInstance();
    httpMgr->handleContactRequest(uid, token, requestId, action);
}

void ContactManager::getContactRequests(const uint64_t &uid, const QString &token, ContactRequestStatus status)
{
    auto httpMgr = HttpMgr::getInstance();
    httpMgr->getContactRequests(uid, token, status);
}

bool ContactManager::getContactInfo(uint64_t uid, Contact &contact) const
{
    if (m_contacts.contains(uid)) {
        contact = m_contacts[uid];
        return true;
    }
    return false;
}

const QMap<uint64_t, Contact>& ContactManager::getAllContacts() const
{
    return m_contacts;
}

QStandardItem* ContactManager::createContactItem(const Contact &contact)
{
    // 创建头像
    QPixmap avatar = createAvatarPixmap(contact.avatar_url, contact.isOnline);
    
    // 创建项目
    auto item = new QStandardItem(QIcon(avatar), contact.name);
    item->setData(QVariant::fromValue(contact.uid), Qt::UserRole);
    
    // 创建自定义小部件
    QWidget *widget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(2, 2, 2, 2);
    
    // 头像
    QLabel *avatarLabel = new QLabel();
    avatarLabel->setFixedSize(32, 32);
    avatarLabel->setPixmap(avatar);
    
    // 名称和状态
    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(0);
    
    QLabel *nameLabel = new QLabel(contact.name);
    nameLabel->setStyleSheet("font-weight: bold;");
    
    QLabel *statusLabel = new QLabel(contact.status);
    statusLabel->setStyleSheet("color: gray; font-size: 9pt;");
    
    textLayout->addWidget(nameLabel);
    textLayout->addWidget(statusLabel);
    
    layout->addWidget(avatarLabel);
    layout->addLayout(textLayout, 1);
    
    // 设置项目文本，但它会被自定义小部件覆盖
    item->setText(contact.name);
    item->setEditable(false);
    
    return item;
}

void ContactManager::processGetContactsResult(const QJsonObject &jsonObj)
{
    m_contacts.clear();
    QJsonArray contactsArray = jsonObj["contacts"].toArray();
    for (const QJsonValue &value : contactsArray) {
        QJsonObject contactObj = value.toObject();
        Contact contact;
        contact.uid = contactObj["uid"].toVariant().toULongLong();
        contact.name = contactObj.contains("nickname") && !contactObj["nickname"].toString().isEmpty() ? 
                      contactObj["nickname"].toString() : contactObj["username"].toString();
        contact.avatar_url = contactObj["avatar_url"].toString();
        contact.status = contactObj["status"].toString();
        contact.isOnline = contact.status == "online";
        contact.signature = contactObj["signature"].toString();
        m_contacts.insert(contact.uid, contact);
    }
    emit contactsUpdated();
}

void ContactManager::processAddContactResult(const QJsonObject &jsonObj)
{
    // 联系人添加成功，重新获取联系人列表
    // 注意：这里可以改进为直接添加新联系人而不是重新获取整个列表
    emit contactAdded(jsonObj["friend_id"].toVariant().toULongLong());
}

void ContactManager::processSearchUserResult(const QJsonObject &jsonObj)
{
    QJsonArray usersArray = jsonObj["users"].toArray();
    // 发出搜索完成信号
    emit userSearchCompleted(usersArray);
}

void ContactManager::processSendContactRequestResult(const QJsonObject &jsonObj)
{
    uint64_t requestId = jsonObj["request_id"].toVariant().toULongLong();
    emit contactRequestSent(requestId);
}

void ContactManager::processHandleContactRequestResult(const QJsonObject &jsonObj)
{
    uint64_t action = jsonObj["action"].toVariant().toLongLong();
    uint64_t requestId = jsonObj["request_id"].toVariant().toULongLong();
    emit contactRequestHandled(requestId, action);
}

void ContactManager::processGetContactRequestsResult(const QJsonObject &jsonObj)
{
    // 清空当前请求列表
    m_contactRequests.clear();
    
    // 解析请求列表
    QJsonArray requestsArray = jsonObj["requests"].toArray();
    for (const QJsonValue &value : requestsArray) {
        QJsonObject requestObj = value.toObject();
        
        ContactRequest request;
        request.request_id = requestObj["request_id"].toVariant().toULongLong();
        request.requester_id = requestObj["requester_id"].toVariant().toULongLong();
        request.requester_name = requestObj["requester_name"].toString();
        request.requester_nickname = requestObj["requester_nickname"].toString();
        request.requester_avatar = requestObj["requester_avatar"].toString();
        request.request_message = requestObj["request_message"].toString();
        request.status = static_cast<ContactRequestStatus>(requestObj["status"].toInt());
        request.created_at = requestObj["created_at"].toString();
        request.updated_at = requestObj["updated_at"].toString();
        
        m_contactRequests.append(request);
    }
    
    // 发出请求列表更新信号
    emit contactRequestsUpdated();
}

QPixmap ContactManager::createAvatarPixmap(const QString &avatarUrl, bool isOnline)
{
    // 创建圆形头像
    QPixmap pixmap(avatarUrl.isEmpty() ? ":/resource/image/default-avatar.svg" : avatarUrl);
    
    QPixmap rounded(pixmap.size());
    rounded.fill(Qt::transparent);
    
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QPainterPath path;
    path.addEllipse(0, 0, pixmap.width(), pixmap.height());
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, pixmap.scaled(pixmap.width(), pixmap.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    
    // 添加在线状态指示器
    if (isOnline) {
        painter.setBrush(QBrush(Qt::green));
        painter.setPen(QPen(Qt::white, 1));
        painter.drawEllipse(pixmap.width() - 10, pixmap.height() - 10, 8, 8);
    }
    
    return rounded;
}

const QList<ContactRequest>& ContactManager::getContactRequestsList() const
{
    return m_contactRequests;
} 