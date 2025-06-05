#ifndef LOGINDIALOG_HPP
#define LOGINDIALOG_HPP

#include <QDialog>
#include "global.h"
#include <functional>

namespace Ui {
class LoginDialog;
}
class QLineEdit;
class QLabel;
class QJsonObject;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    void showEvent(QShowEvent *event) override;
    void initHttpHandlers();

signals:
    void loginSuccess(const ServerInfo &serverInfo);

private slots:
    // 登录页面槽函数
    void on_login_btn_clicked();
    void on_to_register_btn_clicked();
    void on_forget_passwd_btn_clicked();
    void slot_login_finish(ReqId id, const QString& res, ErrorCodes err);

    // 注册页面槽函数
    void on_register_confirm_btn_clicked();
    void on_reg_captcha_btn_clicked();
    void on_to_login_btn_clicked();
    void slot_reg_mod_finish(ReqId id, const QString& res, ErrorCodes err);

    // 重置密码页面槽函数
    void on_reset_captcha_btn_clicked();
    void on_reset_confirm_btn_clicked();
    void on_reset_to_login_btn_clicked();
    void slot_reset_mod_finish(ReqId id, const QString& res, ErrorCodes err);

    // 密码显示/隐藏槽函数
    void toggleLoginPasswordVisibility();
    void toggleRegisterPasswordVisibility();
    void toggleRegisterConfirmPasswordVisibility();
    void toggleResetPasswordVisibility();
    void toggleResetConfirmPasswordVisibility();

private:
    void showTip(QLabel *label, const QString &tip, bool err = true);
    bool validateRegisterInput();
    bool validateResetConfirmInput();
    void switchToLoginPage();
    void switchToRegisterPage();
    void switchToResetPage();
    void setupPasswordVisibilityToggle();

private:
    Ui::LoginDialog *ui;
    QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers;

    // 密码可见性状态
    bool m_loginPasswordVisible = false;
    bool m_registerPasswordVisible = false;
    bool m_registerConfirmPasswordVisible = false;
    bool m_resetPasswordVisible = false;
    bool m_resetConfirmPasswordVisible = false;
};

#endif // LOGINDIALOG_HPP
