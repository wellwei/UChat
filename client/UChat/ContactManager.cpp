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

void ContactManager::addContact(const uint64_t &uid, const QString &token, const uint64_t &friendId)
{
    auto httpMgr = HttpMgr::getInstance();
    httpMgr->addContact(uid, token, friendId);
}

void ContactManager::searchUser(const uint64_t &uid, const QString &token, const QString &keyword)
{
    auto httpMgr = HttpMgr::getInstance();
    httpMgr->searchUser(uid, token, keyword);
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
    QStandardItem *item = new QStandardItem();
    item->setData(QVariant::fromValue(contact.uid), Qt::UserRole);
    
    // 创建自定义小部件
    QWidget *widget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(2, 2, 2, 2);
    
    // 头像
    QLabel *avatarLabel = new QLabel();
    avatarLabel->setFixedSize(32, 32);
    QPixmap avatarPixmap = createAvatarPixmap(contact.avatarPath, contact.isOnline);
    avatarLabel->setPixmap(avatarPixmap);
    
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
    // 清空当前联系人列表
    m_contacts.clear();
    
    // 解析联系人列表
    QJsonArray contactsArray = jsonObj["contacts"].toArray();
    for (const QJsonValue &value : contactsArray) {
        QJsonObject contactObj = value.toObject();
        
        Contact contact;
        contact.uid = contactObj["uid"].toVariant().toULongLong();
        contact.name = contactObj.contains("nickname") && !contactObj["nickname"].toString().isEmpty() ?
                      contactObj["nickname"].toString() : contactObj["username"].toString();
        contact.avatarPath = contactObj["avatar_url"].toString();
        contact.signature = contactObj["signature"].toString();
        
        // 判断在线状态 (0=离线, 1=在线)
        int statusCode = contactObj["status"].toInt();
        contact.isOnline = statusCode > 0;
        
        // 设置状态文本
        switch (statusCode) {
            case 0:
                contact.status = "离线";
                break;
            case 1:
                contact.status = "在线";
                break;
            case 2:
                contact.status = "忙碌";
                break;
            case 3:
                contact.status = "离开";
                break;
            case 4:
                contact.status = "隐身";
                break;
            case 5:
                contact.status = "请勿打扰";
                break;
            default:
                contact.status = "未知状态";
                break;
        }
        
        m_contacts[contact.uid] = contact;
    }
    
    // 发出联系人更新信号
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

QPixmap ContactManager::createAvatarPixmap(const QString &avatarPath, bool isOnline)
{
    QPixmap avatarPixmap;
    
    if (!avatarPath.isEmpty() && QFile::exists(avatarPath)) {
        avatarPixmap.load(avatarPath);
    } else {
        // 创建随机颜色的默认头像
        QColor color(
            QRandomGenerator::global()->bounded(100, 240),
            QRandomGenerator::global()->bounded(100, 240),
            QRandomGenerator::global()->bounded(100, 240)
        );
        
        avatarPixmap = QPixmap(40, 40);
        avatarPixmap.fill(Qt::transparent);
        
        QPainter painter(&avatarPixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        
        QPainterPath path;
        path.addEllipse(0, 0, 40, 40);
        painter.setClipPath(path);
        painter.fillRect(0, 0, 40, 40, color);
        
        // 可以在这里添加头像的首字母
    }
    
    // 裁剪为圆形
    QPixmap roundedPixmap(40, 40);
    roundedPixmap.fill(Qt::transparent);
    
    QPainter painter(&roundedPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QPainterPath path;
    path.addEllipse(0, 0, 40, 40);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, avatarPixmap.scaled(40, 40, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    
    // 添加在线状态指示器
    if (isOnline) {
        painter.setBrush(QBrush(Qt::green));
        painter.setPen(QPen(Qt::white, 1));
        painter.drawEllipse(30, 30, 8, 8);
    }
    
    return roundedPixmap;
} 