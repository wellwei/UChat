//
// Created by wellwei on 2025/3/28.
//

#ifndef UCHAT_HTTPMGR_H
#define UCHAT_HTTPMGR_H

#include "global.h"
#include "Singleton.h"

#include <QNetworkAccessManager>

class HttpMgr : public QObject, public Singleton<HttpMgr>, public std::enable_shared_from_this<HttpMgr>
{
    Q_OBJECT

public:
    ~HttpMgr();

    void PostHttpReq(const QUrl &url, const QJsonObject &jsonObj, ReqId reqId, Modules module);

    // 登录接口
    void login(const QString &username, const QString &password);

    // 注册接口
    void registerUser(const QString &username, const QString &password, const QString &email, const QString &captcha);

    // 重置密码接口
    void resetPassword(const QString &email, const QString &captcha, const QString &newPassword);

    // 获取验证码
    void getVerify(const QString &email);

    void getPasswordResetCode(const QString &email);

    // 获取ChatServer服务器信息
    void getChatServer(const uint64_t &uid, const QString &token);
    
    // 获取用户资料
    void getUserProfile(const uint64_t &uid, const QString &token);
    
    // 获取联系人列表
    void getContacts(const uint64_t &uid, const QString &token);
    
    // 搜索用户
    void searchUser(const uint64_t &uid, const QString &token, const QString &keyword);

    // 新增：联系人请求相关方法
    void sendContactRequest(const uint64_t &uid, const QString &token, const uint64_t &addresseeId, const QString &message);
    void handleContactRequest(const uint64_t &uid, const QString &token, const uint64_t &requestId, ContactRequestStatus action);
    void getContactRequests(const uint64_t &uid, const QString &token, ContactRequestStatus status);

private:
    friend class Singleton<HttpMgr>;
    HttpMgr();

    QNetworkAccessManager _networkManager;

signals:
    void sig_http_finish(ReqId id, const QString& res, ErrorCodes err, Modules module);
    void sig_reg_mod_finish(ReqId id, const QString& res, ErrorCodes err);
    void sig_login_finish(ReqId id, const QString& res, ErrorCodes err);
    void sig_reset_mod_finish(ReqId id, const QString& res, ErrorCodes err);
    void sig_chat_mod_finish(ReqId id, const QString& res, ErrorCodes err);

private slots:
    void slot_http_finish(ReqId id, const QString& res, ErrorCodes err, Modules module);
};


#endif //UCHAT_HTTPMGR_H
