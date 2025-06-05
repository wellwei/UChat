#include "mainwindow.hpp"
#include "global.h"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>
#include <QSettings>

// 全局变量已在global.cpp中定义，这里不需要再定义
// std::function<void(QWidget *)> repolish;
// QString GATE_SERVER_URL;

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 设置应用风格为 Fusion，能更好的支持样式表
    QApplication::setStyle(QStyleFactory::create("Fusion")); 

    // 设置应用图标
    QApplication::setWindowIcon(QIcon(":/resource/image/uchat.ico"));
    // 定义repolish函数实现
    repolish = [](QWidget *widget) {
        if (widget) {
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
            // 对子控件也应用样式刷新
            for (QObject *child : widget->children()) {
                QWidget *childWidget = qobject_cast<QWidget *>(child);
                if (childWidget) {
                    repolish(childWidget);
                }
            }
        }
    };

    // 从 qrc 中读取 config.ini 文件
    const QSettings settings(":/config/config.ini", QSettings::IniFormat);
    GATE_SERVER_URL =
            "http://" + settings.value("GateServer/host").toString() + ":" + settings.value("GateServer/port").toString();

    MainWindow mainwindow;
    mainwindow.showLoginDialog(); // 先显示登录对话框

    return a.exec();
}
