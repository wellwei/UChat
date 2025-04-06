#include "logindialog.hpp"
#include "ui_logindialog.h"
#include "mainwindow.hpp"
#include "HttpMgr.h"

#include <QMessageBox>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
    , mainWindow(nullptr)
{
    ui->setupUi(this);

    // 设置窗口样式表
    QFile qss(":/style/logindialog.qss");
    if (qss.open(QFile::ReadOnly)) {
        QString style = QLatin1String(qss.readAll());
        this->setStyleSheet(style);
        qss.close();
    } else {
        qDebug() << "Failed to open :/style/stylesheet.qss";
    }

    // 初始显示登录页面
    ui->stackedWidget->setCurrentIndex(0);

    // 连接 HTTP 管理器信号
    connect(HttpMgr::getInstance().get(), &HttpMgr::sig_login_finish, this, &LoginDialog::slot_login_finish);
    connect(HttpMgr::getInstance().get(), &HttpMgr::sig_reg_mod_finish, this, &LoginDialog::slot_reg_mod_finish);

    // 初始化 HTTP 处理器
    initHttpHandlers();

    qDebug() << "LoginDialog constructed";
}

LoginDialog::~LoginDialog()
{
    qDebug() << "LoginDialog destroyed";
    disconnect(HttpMgr::getInstance().get(), &HttpMgr::sig_login_finish, this, &LoginDialog::slot_login_finish);
    disconnect(HttpMgr::getInstance().get(), &HttpMgr::sig_reg_mod_finish, this, &LoginDialog::slot_reg_mod_finish);
    delete ui;
    if (mainWindow) {
        delete mainWindow;
    }
}

void LoginDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    // 默认聚焦到用户名输入框
    if (ui->stackedWidget->currentIndex() == 0) {
        ui->username_edit->setFocus();
    } else {
        ui->reg_username_edit->setFocus();
    }
}

// 登录页面 - 登录按钮点击
void LoginDialog::on_login_btn_clicked()
{
    qDebug() << "Login button clicked";
    QString username = ui->username_edit->text().trimmed();
    QString password = ui->password_edit->text();

    // 输入验证
    if (username.isEmpty()) {
        showLoginTip(tr("请输入用户名"));
        ui->username_edit->setFocus();
        return;
    }

    if (password.isEmpty()) {
        showLoginTip(tr("请输入密码"));
        ui->password_edit->setFocus();
        return;
    }

    // 显示正在登录提示
    showLoginTip(tr("正在登录..."), false);

    // 调用登录接口
    HttpMgr::getInstance()->login(username, password);
}

// 登录页面 - 去注册按钮点击
void LoginDialog::on_to_register_btn_clicked()
{
    qDebug() << "Switch to register page clicked";
    switchToRegisterPage();
}

// 注册页面 - 确认注册按钮点击
void LoginDialog::on_register_confirm_btn_clicked()
{
    qDebug() << "Register confirm button clicked";
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
    showRegisterTip(tr("正在注册..."), false);

    // 调用注册接口
    HttpMgr::getInstance()->registerUser(username, password, email, captcha);
}

// 注册页面 - 获取验证码按钮点击
void LoginDialog::on_reg_captcha_btn_clicked()
{
    auto email = ui->reg_email_edit->text();

    static QRegularExpression regx("^[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\\.[a-zA-Z0-9-.]+$");
    bool match = regx.match(email).hasMatch();
    if (match) {
        // 发送验证码请求
        HttpMgr::getInstance()->getCaptcha(email);
        showRegisterTip(tr("验证码发送中..."), false);
    } else {
        showRegisterTip(tr("请输入有效的邮箱地址"));
    }
}

// 注册页面 - 返回登录按钮点击
void LoginDialog::on_to_login_btn_clicked()
{
    qDebug() << "Switch to login page clicked";
    switchToLoginPage();
}

// 登录请求处理
void LoginDialog::slot_login_finish(ReqId id, const QString &res, ErrorCodes err)
{
    if (err != ErrorCodes::SUCCESS) {
        showLoginTip(tr("网络错误"));
        return;
    }

    // 解析服务器返回的 JSON 数据
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        showLoginTip(tr("JSON 解析错误"));
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();

    auto it = _handlers.find(id);
    if (it != _handlers.end()) {
        // 调用对应的处理函数
        it.value()(jsonObj);
    } else {
        showLoginTip(tr("未知请求 ID"));
    }
}

// 注册请求处理
void LoginDialog::slot_reg_mod_finish(ReqId id, const QString &res, ErrorCodes err)
{
    if (err != ErrorCodes::SUCCESS) {
        showRegisterTip(tr("网络错误"));
        return;
    }

    // 解析服务器返回的 JSON 数据
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        showRegisterTip(tr("JSON 解析错误"));
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();

    auto it = _handlers.find(id);
    if (it != _handlers.end()) {
        // 调用对应的处理函数
        it.value()(jsonObj);
    } else {
        showRegisterTip(tr("未知请求 ID"));
    }
}

// 显示登录页面的提示信息
void LoginDialog::showLoginTip(const QString &tip, bool err)
{
    ui->login_err_tip_label->setText(tip);
    if (err) {
        ui->login_err_tip_label->setProperty("state", "error");
    } else {
        ui->login_err_tip_label->setProperty("state", "normal");
    }
    repolish(ui->login_err_tip_label);
}

// 显示注册页面的提示信息
void LoginDialog::showRegisterTip(const QString &tip, bool err)
{
    ui->register_err_tip_label->setText(tip);
    if (err) {
        ui->register_err_tip_label->setProperty("state", "error");
    } else {
        ui->register_err_tip_label->setProperty("state", "normal");
    }
    repolish(ui->register_err_tip_label);
}

// 切换到登录页面
void LoginDialog::switchToLoginPage()
{
    // 清空注册页面的输入
    ui->reg_username_edit->clear();
    ui->reg_password_edit->clear();
    ui->reg_confirm_password_edit->clear();
    ui->reg_email_edit->clear();
    ui->reg_captcha_edit->clear();
    ui->register_err_tip_label->clear();

    // 切换到登录页面
    ui->stackedWidget->setCurrentIndex(0);
    ui->username_edit->setFocus();
}

// 切换到注册页面
void LoginDialog::switchToRegisterPage()
{
    // 清空注册页面的输入
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

// 验证注册输入
bool LoginDialog::validateRegisterInput()
{
    QString username = ui->reg_username_edit->text().trimmed();
    QString password = ui->reg_password_edit->text();
    QString confirmPassword = ui->reg_confirm_password_edit->text();
    QString email = ui->reg_email_edit->text().trimmed();
    QString captcha = ui->reg_captcha_edit->text().trimmed();

    // 验证用户名
    if (username.isEmpty()) {
        showRegisterTip(tr("请输入用户名"));
        ui->reg_username_edit->setFocus();
        return false;
    }

    if (username.length() < 3 || username.length() > 20) {
        showRegisterTip(tr("用户名长度应为 3-20 个字符"));
        ui->reg_username_edit->setFocus();
        return false;
    }

    // 验证密码
    if (password.isEmpty()) {
        showRegisterTip(tr("请输入密码"));
        ui->reg_password_edit->setFocus();
        return false;
    }

    if (password.length() < 6) {
        showRegisterTip(tr("密码长度不能少于 6 个字符"));
        ui->reg_password_edit->setFocus();
        return false;
    }

    // 验证确认密码
    if (confirmPassword != password) {
        showRegisterTip(tr("两次输入的密码不一致"));
        ui->reg_confirm_password_edit->setFocus();
        return false;
    }

    // 验证邮箱
    static QRegularExpression regx("^[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\\.[a-zA-Z0-9-.]+$");
    bool match = regx.match(email).hasMatch();
    if (!match) {
        showRegisterTip(tr("请输入有效的邮箱地址"));
        ui->reg_email_edit->setFocus();
        return false;
    }

    // 验证验证码
    if (captcha.isEmpty()) {
        showRegisterTip(tr("请输入验证码"));
        ui->reg_captcha_edit->setFocus();
        return false;
    }

    return true;
}

// 注册请求响应后的处理
void LoginDialog::initHttpHandlers()
{
    // 登录请求响应处理
    _handlers.insert(ReqId::ID_LOGIN,
        [this](const QJsonObject &jsonObj)
        {
            int error_code = jsonObj.value("error").toInt();

            if (error_code == 0) {
                // 登录成功
                QString username = ui->username_edit->text().trimmed();

                // 创建并显示主窗口
                if (!mainWindow) {
                    mainWindow = new MainWindow();
                }

                // 发送登录成功信号
                emit loginSuccess(username);

                // 隐藏登录窗口并显示主窗口
                this->hide();
                mainWindow->show();
            } else {
                // 登录失败
                QString message = jsonObj.value("message").toString();
                if (message.isEmpty()) {
                    message = tr("用户名或密码错误");
                }
                showLoginTip(message);
            }
        }
    );

    // 验证码请求响应处理
    _handlers.insert(ReqId::ID_GET_CAPTCHA,
        [this](const QJsonObject &jsonObj) {
            int error_code = jsonObj.value("error").toInt();
            if (error_code == 0) {
                showRegisterTip(tr("验证码已发送到邮箱，请注意查收"), false);
            } else {
                QString message = jsonObj.value("message").toString();
                if (message.isEmpty()) {
                    message = tr("验证码发送失败");
                }
                showRegisterTip(message);
            }
        }
    );

    // 注册请求响应处理
    _handlers.insert(ReqId::ID_REGISTER,
        [this](const QJsonObject &jsonObj) {
            int error_code = jsonObj.value("error").toInt();
            if (error_code == 0) {
                // 注册成功
                QString username = ui->reg_username_edit->text().trimmed();

                // 显示注册成功提示
                showRegisterTip(tr("注册成功！正在跳转到登录页面..."), false);

                // 发送注册成功信号
                emit registerSuccess(username);

                // 延迟切换到登录页面
                QTimer::singleShot(1500, this, &LoginDialog::switchToLoginPage);
            } else {
                // 注册失败
                QString message = jsonObj.value("message").toString();
                if (message.isEmpty()) {
                    message = tr("注册失败，请稍后重试");
                }
                showRegisterTip(message);
            }
        }
    );
}
