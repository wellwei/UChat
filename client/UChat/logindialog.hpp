#ifndef LOGINDIALOG_HPP
#define LOGINDIALOG_HPP

#include <QDialog>
#include "global.h"

namespace Ui {
class LoginDialog;
}

class MainWindow;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    void showEvent(QShowEvent *event) override;
    void initHttpHandlers();
    
signals:
    void loginSuccess(const QString &username);
    void registerSuccess(const QString &username);

private slots:
    // 登录页面槽函数
    void on_login_btn_clicked();
    void on_to_register_btn_clicked();
    void slot_login_finish(ReqId id, const QString& res, ErrorCodes err);
    
    // 注册页面槽函数
    void on_register_confirm_btn_clicked();
    void on_reg_captcha_btn_clicked();
    void on_to_login_btn_clicked();
    void slot_reg_mod_finish(ReqId id, const QString& res, ErrorCodes err);

private:
    void showLoginTip(const QString &tip, bool err = true);
    void showRegisterTip(const QString &tip, bool err = true);
    bool validateRegisterInput();
    void switchToLoginPage();
    void switchToRegisterPage();
    
private:
    Ui::LoginDialog *ui;
    QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers;
    MainWindow *mainWindow;
};

#endif // LOGINDIALOG_HPP
