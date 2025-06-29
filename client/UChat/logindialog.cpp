#include "logindialog.hpp"
#include "ui_logindialog.h"
#include "HttpMgr.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

LoginDialog::LoginDialog(QWidget *parent)
        : QDialog(parent), ui(new Ui::LoginDialog) {
    ui->setupUi(this);

    // 设置窗口样式表
    QFile qss(":/resource/style/logindialog.qss");
    if (qss.open(QFile::ReadOnly)) {
        QString style = QLatin1String(qss.readAll());
        this->setStyleSheet(style);
        qss.close();
    } else {
        qDebug() << "Failed to open :/resource/style/logindialog.qss" << qss.errorString();
    }

    // 初始显示登录页面
    ui->stackedWidget->setCurrentIndex(0);

    // 初始化错误提示标签状态
    ui->login_err_tip_label->setProperty("state", "");
    ui->register_err_tip_label->setProperty("state", "");
    ui->reset_err_tip_label->setProperty("state", "");

    // 连接 HTTP 管理器信号
    connect(HttpMgr::getInstance().get(), &HttpMgr::sig_login_finish, this, &LoginDialog::slot_login_finish);
    connect(HttpMgr::getInstance().get(), &HttpMgr::sig_reg_mod_finish, this, &LoginDialog::slot_reg_mod_finish);
    connect(HttpMgr::getInstance().get(), &HttpMgr::sig_reset_mod_finish, this, &LoginDialog::slot_reset_mod_finish);
    connect(HttpMgr::getInstance().get(), &HttpMgr::sig_chat_mod_finish, this, &LoginDialog::slot_chat_mod_finish);

    // 设置密码框显示/隐藏按钮
    setupPasswordVisibilityToggle();

    // 初始化 HTTP 处理器
    initHttpHandlers();
}

LoginDialog::~LoginDialog() {
    qDebug() << "LoginDialog destroyed";
    disconnect(HttpMgr::getInstance().get(), &HttpMgr::sig_login_finish, this, &LoginDialog::slot_login_finish);
    disconnect(HttpMgr::getInstance().get(), &HttpMgr::sig_reg_mod_finish, this, &LoginDialog::slot_reg_mod_finish);
    disconnect(HttpMgr::getInstance().get(), &HttpMgr::sig_reset_mod_finish, this, &LoginDialog::slot_reset_mod_finish);
    disconnect(HttpMgr::getInstance().get(), &HttpMgr::sig_chat_mod_finish, this, &LoginDialog::slot_chat_mod_finish);
    delete ui;
}

void LoginDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    // 根据当前页面设置焦点
    int index = ui->stackedWidget->currentIndex();
    if (index == 0) { // Login Page
        ui->handle_edit->setFocus();
    } else if (index == 1) { // Register Page
        ui->reg_username_edit->setFocus();
    } else if (index == 2) { // Reset Page
        ui->reset_email_edit->setFocus(); // 初始焦点在邮箱
    }
}

/*========================== 登录页面 ==========================*/

// 登录页面 - 登录按钮点击
void LoginDialog::on_login_btn_clicked() {
    qDebug() << "Login button clicked";
    QString handle = ui->handle_edit->text().trimmed();
    QString password = ui->password_edit->text();

    // 输入验证
    if (handle.isEmpty()) {
        showTip(ui->login_err_tip_label, tr("请输入用户名/邮箱"));
        ui->handle_edit->setFocus();
        return;
    }

    if (password.isEmpty()) {
        showTip(ui->login_err_tip_label, tr("请输入密码"));
        ui->password_edit->setFocus();
        return;
    }

    // 显示正在登录提示
    showTip(ui->login_err_tip_label, tr("正在登录..."), false);
    setEnabled(false);

    // 调用登录接口
    HttpMgr::getInstance()->login(handle, password);
}

// 忘记密码按钮点击
void LoginDialog::on_forget_passwd_btn_clicked() {
    switchToResetPage();
}

// 登录页面 - 去注册按钮点击
void LoginDialog::on_to_register_btn_clicked() {
    switchToRegisterPage();
}

// 登录请求处理
void LoginDialog::slot_login_finish(ReqId id, const QString &res, ErrorCodes err) {
    setEnabled(true);
    if (err != ErrorCodes::SUCCESS) {
        showTip(ui->login_err_tip_label, tr("网络错误"));
        return;
    }

    // 解析服务器返回的 JSON 数据
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        showTip(ui->login_err_tip_label, tr("JSON 解析错误"));
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();

    auto it = _handlers.find(id);
    if (it != _handlers.end()) {
        // 调用对应的处理函数
        it.value()(jsonObj);
    } else {
        showTip(ui->login_err_tip_label, tr("未知请求 ID"));
    }
}

/*==============================================================*/
/*========================== 注册页面 ==========================*/
/*
 * 注册页面 - 确认注册按钮点击
 * 注册页面 - 获取验证码按钮点击
 * 注册页面 - 返回登录按钮点击
 * 注册请求处理
 */

void LoginDialog::on_register_confirm_btn_clicked() {
    // 验证输入
    if (!validateRegisterInput()) {
        return;
    }

    // 获取输入信息
    QString username = ui->reg_username_edit->text().trimmed();
    QString password = ui->reg_password_edit->text();
    QString email = ui->reg_email_edit->text().trimmed();
    QString captcha = ui->reg_captcha_edit->text().trimmed();

    // 显示正在注册提示
    showTip(ui->register_err_tip_label, tr("正在注册..."), false);
    setEnabled(false);

    // 调用注册接口
    HttpMgr::getInstance()->registerUser(username, password, email, captcha);
}

// 注册页面 - 获取验证码按钮点击
void LoginDialog::on_reg_captcha_btn_clicked() {
    auto email = ui->reg_email_edit->text();

    static QRegularExpression regx("^[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\\.[a-zA-Z0-9-.]+$");
    bool match = regx.match(email).hasMatch();
    if (match) {
        // 发送验证码请求
        HttpMgr::getInstance()->getVerify(email);
        showTip(ui->register_err_tip_label, tr("验证码发送中..."), false);
        setEnabled(false); // 禁用按钮
    } else {
        showTip(ui->register_err_tip_label, tr("请输入有效的邮箱地址"));
    }
}

// 注册页面 - 返回登录按钮点击
void LoginDialog::on_to_login_btn_clicked() {
    switchToLoginPage();
}

// 注册请求处理
void LoginDialog::slot_reg_mod_finish(ReqId id, const QString &res, ErrorCodes err) {
    setEnabled(true); // 启用注册按钮
    if (err != ErrorCodes::SUCCESS) {
        showTip(ui->register_err_tip_label, tr("网络错误"));
        return;
    }

    // 解析服务器返回的 JSON 数据
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        showTip(ui->register_err_tip_label, tr("JSON 解析错误"));
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();

    auto it = _handlers.find(id);
    if (it != _handlers.end()) {
        // 调用对应的处理函数
        it.value()(jsonObj);
    } else {
        showTip(ui->register_err_tip_label, tr("未知请求 ID"));
    }
}

/*=================================================================*/
/*========================== 重置密码页面 ==========================*/
/*
 * 重置密码页面 - 获取验证码按钮点击
 * 重置密码页面 - 确认重置按钮点击
 * 重置密码页面 - 返回登录按钮点击
 * 重置密码请求处理
 */

void LoginDialog::on_reset_captcha_btn_clicked() {
    auto email = ui->reset_email_edit->text();

    static QRegularExpression regx("^[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\\.[a-zA-Z0-9-.]+$");
    bool match = regx.match(email).hasMatch();
    if (match) {
        // 发送验证码请求
        HttpMgr::getInstance()->getPasswordResetCode(email);
        showTip(ui->reset_err_tip_label, tr("验证码发送中..."), false);
        setEnabled(false);
    } else {
        showTip(ui->reset_err_tip_label, tr("请输入有效的邮箱地址"));
    }
}

void LoginDialog::on_reset_confirm_btn_clicked() {
    if (!validateResetConfirmInput()) {
        return;
    }
    QString email = ui->reset_email_edit->text().trimmed(); // 确认邮箱是否仍需
    QString code = ui->reset_captcha_edit->text().trimmed();
    QString newPassword = ui->reset_new_password_edit->text();

    showTip(ui->reset_err_tip_label, tr("正在重置密码..."), false);
    HttpMgr::getInstance()->resetPassword(email, code, newPassword);
    setEnabled(false);
}

void LoginDialog::on_reset_to_login_btn_clicked() {
    switchToLoginPage();
}

void LoginDialog::slot_reset_mod_finish(ReqId id, const QString &res, ErrorCodes err) {
    setEnabled(true);
    if (err != ErrorCodes::SUCCESS) {
        showTip(ui->reset_err_tip_label, tr("网络错误"));
        return;
    }

    // 解析 JSON (与另外两个槽类似)
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        showTip(ui->reset_err_tip_label, tr("JSON 解析错误"));
        return;
    }
    QJsonObject jsonObj = jsonDoc.object();

    auto it = _handlers.find(id);
    if (it != _handlers.end()) {
        it.value()(jsonObj);
    } else {
        showTip(ui->reset_err_tip_label, tr("未知请求 ID"));
    }
}

/*==============================================================*/
/*==============================================================*/

// 设置提示标签
void LoginDialog::showTip(QLabel *label, const QString &tip, bool err) {
    label->setText(tip);
    if (err) {
        label->setProperty("state", "error");
    } else {
        label->setProperty("state", "success");
    }

    repolish(label);
}

// 切换到登录页面
void LoginDialog::switchToLoginPage() {
    // 清空输入框
    ui->handle_edit->clear();
    ui->password_edit->clear();
    ui->login_err_tip_label->clear();
    // 切换到登录页面
    ui->stackedWidget->setCurrentIndex(0);
    ui->handle_edit->setFocus();
}

// 切换到注册页面
void LoginDialog::switchToRegisterPage() {
    // 清空输入框
    ui->reg_username_edit->clear();
    ui->reg_password_edit->clear();
    ui->reg_confirm_password_edit->clear();
    ui->reg_email_edit->clear();
    ui->reg_captcha_edit->clear();
    ui->register_err_tip_label->clear();
    // 切换到注册页面
    ui->stackedWidget->setCurrentIndex(1);
    ui->reg_username_edit->setFocus();
}

// 切换到重置密码页面
void LoginDialog::switchToResetPage() {
    // 清空输入框
    ui->reset_email_edit->clear();
    ui->reset_captcha_edit->clear();
    ui->reset_new_password_edit->clear();
    ui->reset_confirm_password_edit->clear();
    ui->reset_err_tip_label->clear();
    // 切换到重置密码页面
    ui->stackedWidget->setCurrentIndex(2);
    ui->reset_email_edit->setFocus();
}

// 验证注册输入
bool LoginDialog::validateRegisterInput() {
    QString username = ui->reg_username_edit->text().trimmed();
    QString password = ui->reg_password_edit->text();
    QString confirmPassword = ui->reg_confirm_password_edit->text();
    QString email = ui->reg_email_edit->text().trimmed();
    QString captcha = ui->reg_captcha_edit->text().trimmed();

    // 验证用户名
    if (username.isEmpty()) {
        showTip(ui->register_err_tip_label, tr("请输入用户名"));
        ui->reg_username_edit->setFocus();
        return false;
    }

    if (username.length() < 6 || username.length() > 16) {
        showTip(ui->register_err_tip_label, tr("用户名长度应为 6-16 个字符"));
        ui->reg_username_edit->setFocus();
        return false;
    }

    // 验证密码
    if (password.isEmpty()) {
        showTip(ui->register_err_tip_label, tr("请输入密码"));
        ui->reg_password_edit->setFocus();
        return false;
    }

    if (password.length() < 8 || password.length() > 20) {
        showTip(ui->register_err_tip_label, tr("密码长度应为 8-20 个字符"));
        ui->reg_password_edit->setFocus();
        return false;
    }

    // 8-20，包含字母、数字和特殊字符（如  !@#$%^&*()_+-)
    static QRegularExpression passwd_regx(R"((?=.*[a-zA-Z])(?=.*\d)[a-zA-Z\d!@#$%^&*()_+\-=]{8,20})");
    if (!passwd_regx.match(password).hasMatch()) {
        showTip(ui->register_err_tip_label, tr("密码格式不正确 (需包含大小写字母和数字)"));
        ui->reg_password_edit->setFocus();
        return false;
    }

    // 验证确认密码
    if (confirmPassword != password) {
        showTip(ui->register_err_tip_label, tr("两次输入的密码不一致"));
        ui->reg_confirm_password_edit->setFocus();
        return false;
    }

    // 验证邮箱
    static QRegularExpression email_regx("^[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\\.[a-zA-Z0-9-.]+$");
    bool match = email_regx.match(email).hasMatch();
    if (!match) {
        showTip(ui->register_err_tip_label, tr("请输入有效的邮箱地址"));
        ui->reg_email_edit->setFocus();
        return false;
    }

    // 验证验证码
    if (captcha.isEmpty()) {
        showTip(ui->register_err_tip_label, tr("请输入验证码"));
        ui->reg_captcha_edit->setFocus();
        return false;
    }

    return true;
}

// 验证确认重置的输入
bool LoginDialog::validateResetConfirmInput() {
    QString email = ui->reset_email_edit->text().trimmed();
    QString code = ui->reset_captcha_edit->text().trimmed();
    QString password = ui->reset_new_password_edit->text();
    QString confirmPassword = ui->reset_confirm_password_edit->text();

    // 验证邮箱
    static QRegularExpression email_regx("^[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\\.[a-zA-Z0-9-.]+$");
    bool match = email_regx.match(email).hasMatch();
    if (!match) {
        showTip(ui->reset_err_tip_label, tr("请输入有效的邮箱地址"));
        ui->reset_email_edit->setFocus();
        return false;
    }

    // 验证验证码
    if (code.isEmpty()) {
        showTip(ui->reset_err_tip_label, tr("请输入验证码"));
        ui->reset_captcha_edit->setFocus();
        return false;
    }

    // 验证新密码 (复用注册时的验证逻辑)
    if (password.isEmpty()) {
        showTip(ui->reset_err_tip_label, tr("请输入新密码"));
        ui->reset_new_password_edit->setFocus();
        return false;
    }
    if (password.length() < 8 || password.length() > 20) {
        showTip(ui->reset_err_tip_label, tr("新密码长度应为 8-20 个字符"));
        ui->reset_new_password_edit->setFocus();
        return false;
    }
    static QRegularExpression passwd_regx(R"((?=.*[a-zA-Z])(?=.*\d)[a-zA-Z\d!@#$%^&*()_+\-=]{8,20})");
    if (!passwd_regx.match(password).hasMatch()) {
        showTip(ui->reset_err_tip_label, tr("新密码格式不正确 (需包含大小写字母和数字)"));
        ui->reset_new_password_edit->setFocus();
        return false;
    }

    // 验证确认密码
    if (confirmPassword != password) {
        showTip(ui->reset_err_tip_label, tr("两次输入的密码不一致"));
        ui->reset_confirm_password_edit->setFocus();
        return false;
    }

    return true;
}

// 注册请求响应后的处理
void LoginDialog::initHttpHandlers() {
    // 登录请求响应处理
    _handlers.insert(ReqId::ID_LOGIN, [this](const QJsonObject &jsonObj) {
        int error_code = jsonObj.value("error").toInt();

        if (error_code == 0) {
            // 登录成功，开始获取聊天服务器信息和用户资料
            processLoginSuccess(jsonObj);
        } else {
            // 登录失败
            showTip(ui->login_err_tip_label, getErrorMessage(static_cast<ErrorCodes>(error_code)));
        }
    });

    // 验证码请求响应处理
    _handlers.insert(ReqId::ID_GET_VERIFYCODE, [this](const QJsonObject &jsonObj) {
        int error_code = jsonObj.value("error").toInt();
        if (error_code == 0) {
            showTip(ui->register_err_tip_label, tr("验证码已发送到邮箱，请注意查收"), false);
            // 启动定时器
            ui->reg_captcha_btn->startCountdown(60);
        } else {
            showTip(ui->register_err_tip_label, getErrorMessage(static_cast<ErrorCodes>(error_code)));
        }
    });

    // 注册请求响应处理
    _handlers.insert(ReqId::ID_REGISTER, [this](const QJsonObject &jsonObj) {
        int error_code = jsonObj.value("error").toInt();
        if (error_code == 0) {
            // 注册成功
            QString username = ui->reg_username_edit->text().trimmed();

            // 显示注册成功提示
            showTip(ui->register_err_tip_label, tr("注册成功！正在跳转到登录页面..."), false);

            // 延迟切换到登录页面
            QTimer::singleShot(1500, this, &LoginDialog::switchToLoginPage);
        } else {
            // 注册失败
            showTip(ui->register_err_tip_label, getErrorMessage(static_cast<ErrorCodes>(error_code)));
        }
    });

    // 重置密码 - 获取验证码响应处理
    _handlers.insert(ReqId::ID_FORGET_PWD_REQUEST_CODE, [this](const QJsonObject &jsonObj) {
        int error_code = jsonObj.value("error").toInt();
        if (error_code == 0) {
            showTip(ui->reset_err_tip_label, tr("验证码已发送到邮箱，请注意查收"), false);
            // 启动定时器
            ui->reset_captcha_btn->startCountdown(60);
        } else {
            showTip(ui->reset_err_tip_label, getErrorMessage(static_cast<ErrorCodes>(error_code)));
        }
    });

    // 重置密码 - 确认重置响应处理
    _handlers.insert(ReqId::ID_RESET_PASSWORD, [this](const QJsonObject &jsonObj) {
        int error_code = jsonObj.value("error").toInt();
        if (error_code == 0) {
            showTip(ui->reset_err_tip_label, tr("密码重置成功！正在跳转到登录页面..."), false);
            QTimer::singleShot(1500, this, &LoginDialog::switchToLoginPage);
        } else {
            showTip(ui->reset_err_tip_label, getErrorMessage(static_cast<ErrorCodes>(error_code)));
        }
    });
}

// 设置密码框显示/隐藏按钮
void LoginDialog::setupPasswordVisibilityToggle() {
    // 登录页面密码框
    QAction *loginPasswordAction = new QAction(this);
    loginPasswordAction->setIcon(QIcon(":/resource/image/password_invisible.svg"));
    ui->password_edit->addAction(loginPasswordAction, QLineEdit::TrailingPosition);
    connect(loginPasswordAction, &QAction::triggered, this, &LoginDialog::toggleLoginPasswordVisibility);

    // 注册页面密码框
    QAction *registerPasswordAction = new QAction(this);
    registerPasswordAction->setIcon(QIcon(":/resource/image/password_invisible.svg"));
    ui->reg_password_edit->addAction(registerPasswordAction, QLineEdit::TrailingPosition);
    connect(registerPasswordAction, &QAction::triggered, this, &LoginDialog::toggleRegisterPasswordVisibility);

    // 注册页面确认密码框
    QAction *registerConfirmPasswordAction = new QAction(this);
    registerConfirmPasswordAction->setIcon(QIcon(":/resource/image/password_invisible.svg"));
    ui->reg_confirm_password_edit->addAction(registerConfirmPasswordAction, QLineEdit::TrailingPosition);
    connect(registerConfirmPasswordAction, &QAction::triggered, this,
            &LoginDialog::toggleRegisterConfirmPasswordVisibility);

    // 重置密码页面新密码框
    QAction *resetPasswordAction = new QAction(this);
    resetPasswordAction->setIcon(QIcon(":/resource/image/password_invisible.svg"));
    ui->reset_new_password_edit->addAction(resetPasswordAction, QLineEdit::TrailingPosition);
    connect(resetPasswordAction, &QAction::triggered, this, &LoginDialog::toggleResetPasswordVisibility);

    // 重置密码页面确认密码框
    QAction *resetConfirmPasswordAction = new QAction(this);
    resetConfirmPasswordAction->setIcon(QIcon(":/resource/image/password_invisible.svg"));
    ui->reset_confirm_password_edit->addAction(resetConfirmPasswordAction, QLineEdit::TrailingPosition);
    connect(resetConfirmPasswordAction, &QAction::triggered, this,
            &LoginDialog::toggleResetConfirmPasswordVisibility);
}

// 切换登录页面密码显示/隐藏
void LoginDialog::toggleLoginPasswordVisibility() {
    m_loginPasswordVisible = !m_loginPasswordVisible;

    // 切换密码显示模式
    ui->password_edit->setEchoMode(m_loginPasswordVisible ? QLineEdit::Normal : QLineEdit::Password);

    // 更新图标
    QAction *action = ui->password_edit->actions().last();
    action->setIcon(QIcon(m_loginPasswordVisible ? ":/resource/image/password_visible.svg" : ":/resource/image/password_invisible.svg"));
}

// 切换注册页面密码显示/隐藏
void LoginDialog::toggleRegisterPasswordVisibility() {
    m_registerPasswordVisible = !m_registerPasswordVisible;

    // 切换密码显示模式
    ui->reg_password_edit->setEchoMode(m_registerPasswordVisible ? QLineEdit::Normal : QLineEdit::Password);

    // 更新图标
    QAction *action = ui->reg_password_edit->actions().last();
    action->setIcon(QIcon(m_registerPasswordVisible ? ":/resource/image/password_visible.svg" : ":/resource/image/password_invisible.svg"));
}

// 切换注册页面确认密码显示/隐藏
void LoginDialog::toggleRegisterConfirmPasswordVisibility() {
    m_registerConfirmPasswordVisible = !m_registerConfirmPasswordVisible;

    // 切换密码显示模式
    ui->reg_confirm_password_edit->setEchoMode(
            m_registerConfirmPasswordVisible ? QLineEdit::Normal : QLineEdit::Password);

    // 更新图标
    QAction *action = ui->reg_confirm_password_edit->actions().last();
    action->setIcon(QIcon(m_registerConfirmPasswordVisible ? ":/resource/image/password_visible.svg" : ":/resource/image/password_invisible.svg"));
}

// 切换重置密码页面新密码显示/隐藏
void LoginDialog::toggleResetPasswordVisibility() {
    m_resetPasswordVisible = !m_resetPasswordVisible;

    // 切换密码显示模式
    ui->reset_new_password_edit->setEchoMode(m_resetPasswordVisible ? QLineEdit::Normal : QLineEdit::Password);

    // 更新图标
    QAction *action = ui->reset_new_password_edit->actions().last();
    action->setIcon(QIcon(m_resetPasswordVisible ? ":/resource/image/password_visible.svg" : ":/resource/image/password_invisible.svg"));
}

// 切换重置密码页面确认密码显示/隐藏
void LoginDialog::toggleResetConfirmPasswordVisibility() {
    m_resetConfirmPasswordVisible = !m_resetConfirmPasswordVisible;

    // 切换密码显示模式
    ui->reset_confirm_password_edit->setEchoMode(
            m_resetConfirmPasswordVisible ? QLineEdit::Normal : QLineEdit::Password);

    // 更新图标
    QAction *action = ui->reset_confirm_password_edit->actions().last();
    action->setIcon(QIcon(m_resetConfirmPasswordVisible ? ":/resource/image/password_visible.svg" : ":/resource/image/password_invisible.svg"));
}

// 聊天服务相关槽函数
void LoginDialog::slot_chat_mod_finish(ReqId id, const QString &res, ErrorCodes err) {
    setEnabled(true);
    if (err != ErrorCodes::SUCCESS) {
        QString errMsg = id == ID_GET_CHAT_SERVER ? "获取聊天服务器信息失败" : "获取用户资料失败";
        showTip(ui->login_err_tip_label, errMsg);
        return;
    }

    // 解析服务器返回的 JSON 数据
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        showTip(ui->login_err_tip_label, tr("JSON 解析错误"));
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();

    // 根据请求ID处理不同的响应
    if (id == ID_GET_CHAT_SERVER) {
        processChatServerInfo(jsonObj);
    } else if (id == ID_GET_USER_PROFILE) {
        processUserProfile(jsonObj);
    }
}

// 处理登录成功后的数据
void LoginDialog::processLoginSuccess(const QJsonObject &loginResponse) {
    QString token = loginResponse.value("token").toString();
    int uid = loginResponse.value("uid").toInt();

    // 创建ServerInfo对象
    m_clientInfo.token = token;
    m_clientInfo.uid = uid;

    // 请求ChatServer信息
    setEnabled(false); // 禁用界面
    showTip(ui->login_err_tip_label, tr("获取聊天服务器信息..."), false);
    HttpMgr::getInstance()->getChatServer(uid, token);
}

// 处理ChatServer信息
void LoginDialog::processChatServerInfo(const QJsonObject &clientInfo) {
    int error_code = clientInfo.value("error").toInt();
    
    if (error_code == 0) {
        // 获取服务器信息成功
        m_clientInfo.host = clientInfo.value("host").toString();
        m_clientInfo.port = clientInfo.value("port").toString();
        
        // 继续获取用户资料
        setEnabled(false); // 继续禁用界面
        showTip(ui->login_err_tip_label, tr("获取用户资料..."), false);
        HttpMgr::getInstance()->getUserProfile(m_clientInfo.uid, m_clientInfo.token);
    } else {
        // 获取服务器信息失败
        showTip(ui->login_err_tip_label, tr("获取聊天服务器失败"));
    }
}

// 处理用户资料
void LoginDialog::processUserProfile(const QJsonObject &profileInfo) {
    int error_code = profileInfo.value("error").toInt();
    
    if (error_code == 0) {
        // 获取用户资料成功
        QJsonObject profile = profileInfo.value("user_profile").toObject();
        
        // 将用户资料保存到ServerInfo对象中
        m_clientInfo.userProfile.uid = profile.value("uid").toVariant().toULongLong();
        m_clientInfo.userProfile.username = profile.value("username").toString();
        m_clientInfo.userProfile.nickname = profile.value("nickname").toString();
        m_clientInfo.userProfile.avatar_url = profile.value("avatar_url").toString();
        m_clientInfo.userProfile.email = profile.value("email").toString();
        m_clientInfo.userProfile.phone = profile.value("phone").toString();
        m_clientInfo.userProfile.gender = QString::number(profile.value("gender").toInt());
        m_clientInfo.userProfile.signature = profile.value("signature").toString();
        m_clientInfo.userProfile.location = profile.value("location").toString();
        m_clientInfo.userProfile.status = QString::number(profile.value("status").toInt());
        m_clientInfo.userProfile.create_time = profile.value("create_time").toString();
        m_clientInfo.userProfile.update_time = profile.value("update_time").toString();
        
        // 完成登录流程，发射登录成功信号
        emit loginSuccess(m_clientInfo);
        accept();
    } else {
        // 获取用户资料失败
        showTip(ui->login_err_tip_label, tr("获取用户资料失败"));
    }
}