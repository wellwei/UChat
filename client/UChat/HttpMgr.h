//
// Created by wellwei on 2025/3/28.
//

#ifndef UCHAT_HTTPMGR_H
#define UCHAT_HTTPMGR_H

#include "global.h"
#include "Singleton.h"

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
    
    // 获取验证码
    void getCaptcha(const QString &email);

private:
    friend class Singleton<HttpMgr>;
    HttpMgr();

    QNetworkAccessManager _networkManager;

signals:
    void sig_http_finish(ReqId id, const QString& res, ErrorCodes err, Modules module);
    void sig_reg_mod_finish(ReqId id, const QString& res, ErrorCodes err);
    void sig_login_finish(ReqId id, const QString& res, ErrorCodes err);

private slots:
    void slot_http_finish(ReqId id, const QString& res, ErrorCodes err, Modules module);
};


#endif //UCHAT_HTTPMGR_H
