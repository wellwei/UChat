#include <QFile>
#include "mainwindow.hpp"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 设置窗口样式表
    QFile file(":/style/mainwindow.qss");
    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = file.readAll();
        this->setStyleSheet(styleSheet);
        file.close();
    } else {
        qDebug() << "Failed to open :/style/mainwindow.qss";
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
