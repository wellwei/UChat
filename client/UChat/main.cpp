#include "mainwindow.hpp"
#include "logindialog.hpp"

#include <QApplication>
#include <QTranslator>
#include <QFile>
#include <QIcon>
#include <QProxyStyle>
#include <QStyleFactory>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 设置应用风格为 Fusion，能更好的支持样式表
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // 设置应用图标
    QApplication::setWindowIcon(QIcon("://image/uchat.ico"));

    // 创建登录/注册窗口
    LoginDialog loginDialog;
    
    // 显示登录窗口
    loginDialog.show();

    return a.exec();
}
