/*
 * HttpMgr.cpp
 * 本文件实现 HttpMgr 类，负责处理 HTTP 请求和响应
 */

#include "HttpMgr.h"

HttpMgr::HttpMgr() {
    // 连接 http 请求和完成信号，信号槽机制保证队列消费
    connect(this, &HttpMgr::sig_http_finish, this, &HttpMgr::slot_http_finish);
}

HttpMgr::~HttpMgr() {
    // 析构函数
    qDebug() << "HttpMgr destroyed";
}

void HttpMgr::PostHttpReq(const QUrl &url, const QJsonObject &jsonObj, ReqId reqId, Modules module) {
    // 创建一个 HTTP POST 请求，并设置请求头和请求体

    QByteArray postData = QJsonDocument(jsonObj).toJson();

    // 通过 url 构造请求
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(postData.size()));

    // 发送请求，并处理响应，获取自己的智能指针，构造伪闭包增加智能指针引用计数
    auto self = shared_from_this();
    QNetworkReply *reply = _networkManager.post(request, postData);

    // _networkManager.post 是异步的，可能不会立即执行，但是其会返回一个 QNetworkReply 对象，当请求完成时会发出 finished 信号

    /** 上面使用 shared_from_this() 来获取当前对象的 shared_ptr 是为了保持生命期
      * 如果 HttpMgr 对象在 lambda 执行之前就被销毁了（例如，持有 HttpMgr 对象的指针或智能指针被释放了），
      * 那么 lambda 内部如果直接或间接（比如通过捕获的 this 指针）访问 HttpMgr 的成员（如 sig_http_finish 信号），就会导致悬空指针访问，引发程序崩溃（Use-After-Free）。
      * 所以使用 shared_from_this() 来确保在 lambda 执行期间 HttpMgr 对象的生命周期是有效的。
      */
    QObject::connect(reply, &QNetworkReply::finished, [reply, self, reqId, module]() {
        // 处理响应

        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Error: " << reply->errorString();

            emit self->sig_http_finish(reqId, "", ErrorCodes::NETWORK_ERROR, module);

            reply->deleteLater();
            return;
        }

        // 读取响应数据
        QString res = reply->readAll();

        emit self->sig_http_finish(reqId, res, ErrorCodes::SUCCESS, module);

        reply->deleteLater();
        return;
    });
}

void HttpMgr::login(const QString &username, const QString &password) {
    QJsonObject jsonObj;
    jsonObj["username"] = username;
    jsonObj["password"] = password;

    QUrl url(GATE_SERVER_URL + "/login");
    PostHttpReq(url, jsonObj, ReqId::ID_LOGIN, Modules::LOGINMOD);
}

void HttpMgr::registerUser(const QString &username, const QString &password, const QString &email,
                           const QString &verify_code) {
    QJsonObject jsonObj;
    jsonObj["username"] = username;
    jsonObj["password"] = password;
    jsonObj["email"] = email;
    jsonObj["verify_code"] = verify_code;

    QUrl url(GATE_SERVER_URL + "/register");
    PostHttpReq(url, jsonObj, ReqId::ID_REGISTER, Modules::REGISTERMOD);
}

void HttpMgr::getVerify(const QString &email) {
    QJsonObject jsonObj;
    jsonObj["email"] = email;

    QUrl url(GATE_SERVER_URL + "/get_verifycode");
    PostHttpReq(url, jsonObj, ReqId::ID_GET_VERIFYCODE, Modules::REGISTERMOD);
}

void HttpMgr::resetPassword(const QString &email, const QString &captcha, const QString &newPassword) {
    QJsonObject jsonObj;
    jsonObj["email"] = email;
    jsonObj["verify_code"] = captcha;
    jsonObj["new_password"] = newPassword;

    QUrl url(GATE_SERVER_URL + "/reset_password");
    PostHttpReq(url, jsonObj, ReqId::ID_RESET_PASSWORD, Modules::RESETMOD);
}

void HttpMgr::getPasswordResetCode(const QString &email) {
    QJsonObject jsonObj;
    jsonObj["email"] = email;

    QUrl url(GATE_SERVER_URL + "/get_verifycode");
    PostHttpReq(url, jsonObj, ReqId::ID_FORGET_PWD_REQUEST_CODE, Modules::RESETMOD);
}

void HttpMgr::slot_http_finish(ReqId id, const QString &res, ErrorCodes err, Modules module) {
    if (module == Modules::REGISTERMOD) {
        emit sig_reg_mod_finish(id, res, err);
    } else if (module == Modules::LOGINMOD) {
        emit sig_login_finish(id, res, err);
    } else if (module == Modules::RESETMOD) {
        emit sig_reset_mod_finish(id, res, err);
    }
}
