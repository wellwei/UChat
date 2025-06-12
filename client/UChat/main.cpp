#include "mainwindow.hpp"
#include "global.h"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>
#include <QSettings>
#include <QPalette>

// 全局变量已在global.cpp中定义，这里不需要再定义
// std::function<void(QWidget *)> repolish;
// QString GATE_SERVER_URL;

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 设置应用风格为 Fusion，能更好的支持样式表
    QApplication::setStyle(QStyleFactory::create("Fusion")); 
    
    // 强制使用浅色模式
    QPalette lightPalette;
    lightPalette.setColor(QPalette::Window, QColor(255, 255, 255));
    lightPalette.setColor(QPalette::WindowText, QColor(0, 0, 0));
    lightPalette.setColor(QPalette::Base, QColor(255, 255, 255));
    lightPalette.setColor(QPalette::AlternateBase, QColor(240, 240, 240));
    lightPalette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    lightPalette.setColor(QPalette::ToolTipText, QColor(0, 0, 0));
    lightPalette.setColor(QPalette::Text, QColor(0, 0, 0));
    lightPalette.setColor(QPalette::Button, QColor(240, 240, 240));
    lightPalette.setColor(QPalette::ButtonText, QColor(0, 0, 0));
    lightPalette.setColor(QPalette::BrightText, QColor(255, 0, 0));
    lightPalette.setColor(QPalette::Link, QColor(0, 120, 212));
    lightPalette.setColor(QPalette::Highlight, QColor(0, 120, 212));
    lightPalette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    
    // 禁用系统颜色主题跟随
    QApplication::setDesktopSettingsAware(false);
    QApplication::setPalette(lightPalette);

    // 设置应用图标
    QApplication::setWindowIcon(QIcon(":/resource/image/uchat.ico"));

    // 从 qrc 中读取 config.ini 文件
    const QSettings settings(":/config/config.ini", QSettings::IniFormat);
    GATE_SERVER_URL =
            "http://" + settings.value("GateServer/host").toString() + ":" + settings.value("GateServer/port").toString();

    MainWindow mainwindow;
    mainwindow.showLoginDialog(); // 先显示登录对话框

    return QApplication::exec();
}
