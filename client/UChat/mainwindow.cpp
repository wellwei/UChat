#include <QFile>
#include <QCloseEvent>
#include <QMessageBox>
#include <QIcon>
#include <QFileDialog>
#include <QDebug>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QRandomGenerator>
#include <QKeyEvent>

#include "mainwindow.hpp"
#include "./ui_mainwindow.h"
#include "logindialog.hpp"
#include "ChatManager.h"
#include "ContactManager.h"
#include "ChatUIManager.h"
#include "AddContactDialog.h"
#include "MessageDelegate.h"
#include "ChatMessageModel.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isConnected(false)
    , m_contactsModel(nullptr)
    , m_addContactDialog(nullptr)
    , m_trayIcon(nullptr)
{
    ui->setupUi(this);

    // 设置窗口样式表
    QFile file(":/resource/style/mainwindow.qss");
    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = file.readAll();
        this->setStyleSheet(styleSheet);
        file.close();
    } else {
        qDebug() << "Failed to open :/resource/style/mainwindow.qss";
    }
    
    // 初始化UI和事件处理
    initUI();
    initTrayIcon();
    setupSignalsAndSlots();
    
    // 默认显示聊天页面
    ui->centralStackedWidget->setCurrentIndex(0);
    ui->chatNavButton->setChecked(true);
    
    // 添加事件过滤器用于处理特定的键盘事件
    ui->messageInputEdit->installEventFilter(this);
    
    // 初始化聊天UI管理器
    const auto chatUIManager = ChatUIManager::getInstance();
    chatUIManager->initUI(ui->conversationsListWidget, ui->messageListView, ui->chatTargetNameLabel);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initUI()
{
    // 设置窗口标题和图标
    setWindowTitle("UChat - 现代化即时通讯");
    setWindowIcon(QIcon(":/resource/image/uchat.ico"));
    
    // 设置侧边栏图标，使用更现代的图标
    ui->chatNavButton->setIcon(QIcon(":/resource/image/chat-message-pressed.svg"));
    ui->contactsNavButton->setIcon(QIcon(":/resource/image/contacter.svg"));
    ui->groupsNavButton->setIcon(QIcon(":/resource/image/group.svg"));
    ui->settingsNavButton->setIcon(QIcon(":/resource/image/setting.svg"));
    
    // 确保分割器的初始位置合理
    ui->chatSplitter->setSizes(QList<int>() << 280 << 520);
    
    // 创建联系人模型
    m_contactsModel = new QStandardItemModel(this);
    m_contactsModel->setHorizontalHeaderLabels(QStringList() << "联系人");
    ui->contactsTreeView->setModel(m_contactsModel);
    
    // 优化联系人树视图设置
    ui->contactsTreeView->setAnimated(true);
    ui->contactsTreeView->setIndentation(20);
    ui->contactsTreeView->setRootIsDecorated(false);
    ui->contactsTreeView->setHeaderHidden(true);
    ui->contactsTreeView->setUniformRowHeights(true);
    
    // 初始化消息显示区域，现在是 ListView
    ui->messageListView->setSpacing(5); // 为项目之间添加一些间距
    ui->messageListView->setStyleSheet("background-color: #E0E1E2;"); // 设置背景色
    
    // 优化搜索框设置
    setupSearchBoxes();
    
    // 优化会话列表设置
    setupConversationList();
    
    // 设置输入区域初始状态
    ui->messageInputEdit->clear();
    ui->messageInputEdit->setFocus();
    ui->messageInputEdit->setPlaceholderText("输入消息... (Ctrl+Enter 发送)");
    
    // 设置发送按钮初始状态
    updateSendButtonState();
    
    // 设置窗口最小尺寸
    setMinimumSize(900, 650);
    
    // 优化滚动区域
    ui->messageListView->verticalScrollBar()->setStyleSheet("QScrollBar { width: 8px; }");
}

// 优化搜索框设置
void MainWindow::setupSearchBoxes()
{
    // 设置搜索框的动画效果和交互
    ui->searchConversationsLineEdit->setClearButtonEnabled(true);
    ui->searchContactsLineEdit->setClearButtonEnabled(true);
    ui->searchGroupsLineEdit->setClearButtonEnabled(true);
    
    // 改进搜索框的提示文本
    ui->searchConversationsLineEdit->setPlaceholderText("搜索会话...");
    ui->searchContactsLineEdit->setPlaceholderText("搜索联系人...");
    ui->searchGroupsLineEdit->setPlaceholderText("搜索群组...");
    
    // 设置搜索框的快捷键
    ui->searchConversationsLineEdit->addAction(QIcon(":/resource/image/search.svg"), QLineEdit::LeadingPosition);
    ui->searchContactsLineEdit->addAction(QIcon(":/resource/image/search.svg"), QLineEdit::LeadingPosition);
    ui->searchGroupsLineEdit->addAction(QIcon(":/resource/image/search.svg"), QLineEdit::LeadingPosition);
}

// 优化会话列表设置
void MainWindow::setupConversationList()
{
    // 设置会话列表的现代化属性
    ui->conversationsListWidget->setSpacing(1);
    ui->conversationsListWidget->setUniformItemSizes(false);
    ui->conversationsListWidget->setResizeMode(QListView::Adjust);
    ui->conversationsListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->conversationsListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // 设置拖拽支持（未来可能用于排序）
    ui->conversationsListWidget->setDragDropMode(QAbstractItemView::NoDragDrop);
    ui->conversationsListWidget->setDefaultDropAction(Qt::MoveAction);
}

// 更新发送按钮状态
void MainWindow::updateSendButtonState()
{
    bool hasText = !ui->messageInputEdit->toPlainText().trimmed().isEmpty();
    ui->sendButton->setProperty("inputNotEmpty", hasText);
    ui->sendButton->style()->unpolish(ui->sendButton);
    ui->sendButton->style()->polish(ui->sendButton);
    ui->sendButton->update();
}

// 初始化系统托盘图标
void MainWindow::initTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/resource/image/uchat.ico"));
    m_trayIcon->setToolTip("UChat");
    
    m_trayMenu = new QMenu(this);
    QAction *showAction = m_trayMenu->addAction("显示主窗口");
    QAction *quitAction = m_trayMenu->addAction("退出");
    
    connect(showAction, &QAction::triggered, this, &MainWindow::onTrayActionTriggered);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->show();
    
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
}

// 设置信号和槽连接
void MainWindow::setupSignalsAndSlots()
{
    // 导航按钮
    connect(ui->chatNavButton, &QToolButton::clicked, this, &MainWindow::onChatNavButtonClicked);
    connect(ui->contactsNavButton, &QToolButton::clicked, this, &MainWindow::onContactsNavButtonClicked);
    connect(ui->groupsNavButton, &QToolButton::clicked, this, &MainWindow::onGroupsNavButtonClicked);
    connect(ui->settingsNavButton, &QToolButton::clicked, this, &MainWindow::onSettingsNavButtonClicked);
    
    // 聊天功能
    connect(ui->sendButton, &QPushButton::clicked, this, &MainWindow::onSendButtonClicked);
    connect(ui->messageInputEdit, &QTextEdit::textChanged, this, &MainWindow::onMessageInputTextChanged);
    connect(ui->emojiButton, &QPushButton::clicked, this, &MainWindow::onEmojiButtonClicked);
    connect(ui->fileButton, &QPushButton::clicked, this, &MainWindow::onFileButtonClicked);
    connect(ui->screenshotButton, &QPushButton::clicked, this, &MainWindow::onScreenshotButtonClicked);
    
    // 搜索功能
    connect(ui->searchConversationsLineEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchConversations);
    connect(ui->searchContactsLineEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchContacts);
    connect(ui->searchGroupsLineEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchGroups);
    
    // 联系人功能
    connect(ui->contactsTreeView, &QTreeView::clicked, this, &MainWindow::onContactSelected);
    connect(ui->contactsTreeView, &QTreeView::customContextMenuRequested, this, &MainWindow::onContactContextMenu);
    connect(ui->addContactButton, &QPushButton::clicked, this, &MainWindow::onAddContactClicked);
    
    // 初始设置
    ui->conversationsListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->contactsTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // 连接ChatUIManager信号
    auto chatUIManager = ChatUIManager::getInstance();
    connect(chatUIManager.get(), &ChatUIManager::conversationSelected, [this](uint64_t uid) {
        // 处理会话选择事件
    });
    
    // 连接未读消息相关信号
    connect(chatUIManager.get(), &ChatUIManager::unreadCountChanged, this, 
            &MainWindow::onUnreadCountChanged);
    connect(chatUIManager.get(), &ChatUIManager::totalUnreadCountChanged, this, 
            &MainWindow::onTotalUnreadCountChanged);
    connect(chatUIManager.get(), &ChatUIManager::newMessageReceived, this, 
            &MainWindow::onNewMessageReceived);
    
    // 连接ContactManager信号
    auto contactManager = ContactManager::getInstance();
    connect(contactManager.get(), &ContactManager::contactsUpdated, [this]() {
        // 更新联系人树视图
        updateContactsView();
    });
}

// 显示登录对话框
void MainWindow::showLoginDialog()
{
    LoginDialog dialog(this);

    connect(&dialog, &LoginDialog::loginSuccess, 
            [this](const ClientInfo &clientInfo) {
                this->onLoginSuccess(clientInfo);
            });
    
    // 对话框关闭时，如果仍未登陆，则关闭程序
    if (dialog.exec() != QDialog::Accepted) {
        // 检查是否已登录
        if (!m_isConnected) {
            QApplication::quit();
        }
    }
}

void MainWindow::onLoginSuccess(const ClientInfo &clientInfo)
{
    // 保存服务器信息
    m_clientInfo = clientInfo;
    
    // 使用ChatManager连接到聊天服务器
    auto chatManager = ChatManager::getInstance();
    
    // 连接ChatManager的信号
    connect(chatManager.get(), &ChatManager::connected, this, [this, chatManager]() {
        qDebug() << "聊天服务器连接成功";
        updateConnectionStatus(false, "已连接，正在认证...");
        
        // 连接成功后进行认证
        if (!chatManager->authenticate(m_clientInfo.uid, m_clientInfo.token)) {
            qWarning() << "认证请求发起失败";
            updateConnectionStatus(false, "认证请求失败");
            QMessageBox::warning(this, "认证错误", "无法发起认证请求，请稍后重试");
        }
    });
    
    connect(chatManager.get(), &ChatManager::authenticated, this, [this]() {
        qDebug() << "聊天服务器认证成功";
        // 更新UI显示连接状态
        m_isConnected = true;
        updateConnectionStatus(true);
        
        // 初始化管理器
        auto chatUIManager = ChatUIManager::getInstance();
        chatUIManager->setCurrentUserInfo(m_clientInfo.uid, m_clientInfo.userProfile.nickname.isEmpty() ? 
                                         m_clientInfo.userProfile.username : m_clientInfo.userProfile.nickname);
        
        // 获取联系人列表
        auto contactManager = ContactManager::getInstance();
        contactManager->getContacts(m_clientInfo.uid, m_clientInfo.token);
        
        // 显示主窗口
        this->show();
        this->activateWindow();
        this->raise();
    });
    
    connect(chatManager.get(), &ChatManager::error, this, [this](const QString &errorMessage) {
        qDebug() << "聊天连接错误: " << errorMessage;
        // 更新UI以显示错误状态
        m_isConnected = false;
        updateConnectionStatus(false);
        // 显示错误信息
        QMessageBox::warning(this, "连接错误", "与聊天服务器连接出现问题：" + errorMessage);
    });
    
    // 连接消息相关信号
    connect(chatManager.get(), &ChatManager::messageReceived, this, 
            &MainWindow::onMessageReceived);
    
    connect(chatManager.get(), &ChatManager::messageSent, this, 
            &MainWindow::onMessageSent);
    
    // 连接心跳相关信号
    connect(chatManager.get(), &ChatManager::heartbeatTimeout, this, [this]() {
        qDebug() << "心跳超时，连接可能不稳定";
        // 可以在界面上显示连接不稳定的提示
        updateConnectionStatus(false, "连接不稳定");
    });
    
    connect(chatManager.get(), &ChatManager::disconnected, this, [this]() {
        qDebug() << "与服务器断开连接";
        m_isConnected = false;
        updateConnectionStatus(false);
    });
    
    // 连接到聊天服务器
    QStringList hostParts = m_clientInfo.host.split(":");
    QString host;
    quint16 port;
    
    if (hostParts.size() == 2) {
        host = hostParts.at(0);
        port = hostParts.at(1).toUInt();
    } else {
        host = m_clientInfo.host;
        port = m_clientInfo.port.toUInt();
    }
    
    // 发起连接请求
    if (!chatManager->connectToServer(host, port)) {
        qDebug() << "发起聊天服务器连接请求失败";
        updateConnectionStatus(false, "连接请求失败");
        QMessageBox::critical(this, "连接错误", "无法发起到聊天服务器的连接请求，请检查网络设置");
    } else {
        // 连接请求已发起，但连接可能还未建立
        // 实际连接状态会通过connected信号通知
        updateConnectionStatus(false, "正在连接...");
    }
    ui->contactsTreeView->expandAll();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 如果托盘图标可用，则最小化到托盘而不是关闭程序
    if (m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    } else {
        QMainWindow::closeEvent(event);
        event->accept();
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->messageInputEdit) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return && !(keyEvent->modifiers() & Qt::ShiftModifier)) {
                // 按下回车键发送消息，按下Shift+回车换行
                onSendButtonClicked();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// 导航按钮事件处理
void MainWindow::onChatNavButtonClicked()
{
    ui->centralStackedWidget->setCurrentIndex(0);
    
    // 更新按钮图标
    ui->chatNavButton->setIcon(QIcon(":/resource/image/chat-message-pressed.svg"));
    ui->contactsNavButton->setIcon(QIcon(":/resource/image/contacter.svg"));
    ui->groupsNavButton->setIcon(QIcon(":/resource/image/group.svg"));
    ui->settingsNavButton->setIcon(QIcon(":/resource/image/setting.svg"));
}

void MainWindow::onContactsNavButtonClicked()
{
    ui->centralStackedWidget->setCurrentIndex(1);
    
    // 更新按钮图标
    ui->chatNavButton->setIcon(QIcon(":/resource/image/chat-message.svg"));
    ui->contactsNavButton->setIcon(QIcon(":/resource/image/contacter-pressed.svg"));
    ui->groupsNavButton->setIcon(QIcon(":/resource/image/group.svg"));
    ui->settingsNavButton->setIcon(QIcon(":/resource/image/setting.svg"));
    
    // 刷新联系人列表以确保显示最新的联系人（包括新添加的）
    if (m_isConnected && m_clientInfo.uid != 0) {
        auto contactManager = ContactManager::getInstance();
        contactManager->getContacts(m_clientInfo.uid, m_clientInfo.token);
    }
}

void MainWindow::onGroupsNavButtonClicked()
{
    ui->centralStackedWidget->setCurrentIndex(2);
    
    // 更新按钮图标
    ui->chatNavButton->setIcon(QIcon(":/resource/image/chat-message.svg"));
    ui->contactsNavButton->setIcon(QIcon(":/resource/image/contacter.svg"));
    ui->groupsNavButton->setIcon(QIcon(":/resource/image/group-pressed.svg"));
    ui->settingsNavButton->setIcon(QIcon(":/resource/image/setting.svg"));
}

void MainWindow::onSettingsNavButtonClicked()
{
    ui->centralStackedWidget->setCurrentIndex(3);
    
    // 更新按钮图标
    ui->chatNavButton->setIcon(QIcon(":/resource/image/chat-message.svg"));
    ui->contactsNavButton->setIcon(QIcon(":/resource/image/contacter.svg"));
    ui->groupsNavButton->setIcon(QIcon(":/resource/image/group.svg"));
    ui->settingsNavButton->setIcon(QIcon(":/resource/image/setting-pressed.svg"));
}

// 聊天功能实现
void MainWindow::onSendButtonClicked()
{
    QString message = ui->messageInputEdit->toPlainText().trimmed();
    if (message.isEmpty()) {
        return; // 不发送空消息
    }
    
    auto chatUIManager = ChatUIManager::getInstance();
    uint64_t currentChatUid = chatUIManager->getCurrentChatUid();
    if (currentChatUid == 0) {
        return; // 无聊天对象
    }
    
    // 创建消息对象
    ChatMessage chatMessage;
    chatMessage.senderId = m_clientInfo.uid;
    chatMessage.receiverId = currentChatUid;
    chatMessage.content = message;
    chatMessage.type = "text";
    chatMessage.timestamp = QDateTime::currentSecsSinceEpoch();
    chatMessage.isOutgoing = true;
    // userProfile 中没有头像信息，暂时使用默认头像
    chatMessage.avatarPath = ":/resource/image/default-avatar.svg"; // 我方头像
    
    // 直接通过 ChatUIManager 添加消息
    chatUIManager->addMessage(currentChatUid, chatMessage);
    
    // 发送到服务器
    auto chatManager = ChatManager::getInstance();
    if (chatManager->isAuthenticated()) {
        chatManager->sendMessage(currentChatUid, message);
    }
    
    // 清空输入框
    ui->messageInputEdit->clear();
}

void MainWindow::onMessageInputTextChanged()
{
    // 可以在这里添加"正在输入"状态的处理
    QString text = ui->messageInputEdit->toPlainText();
    ui->sendButton->setEnabled(!text.trimmed().isEmpty());
    
    // 使用新的状态更新方法
    updateSendButtonState();
}

void MainWindow::onEmojiButtonClicked()
{
    // TODO: 显示表情选择面板
    QMessageBox::information(this, "功能提示", "表情功能正在开发中");
}

void MainWindow::onFileButtonClicked()
{
    // 打开文件选择对话框
    QString fileName = QFileDialog::getOpenFileName(this, "选择文件");
    if (!fileName.isEmpty()) {
        // TODO: 处理文件发送
        QMessageBox::information(this, "文件发送", QString("文件 %1 发送功能正在开发中").arg(fileName));
    }
}

void MainWindow::onScreenshotButtonClicked()
{
    // TODO: 实现截图功能
    QMessageBox::information(this, "功能提示", "截图功能正在开发中");
}

void MainWindow::onSearchConversations(const QString &text)
{
    // 筛选匹配的会话
    auto chatUIManager = ChatUIManager::getInstance();
    auto conversations = chatUIManager->getConversations();
    
    QMap<uint64_t, Conversation> filteredConversations;
    for (auto it = conversations.begin(); it != conversations.end(); ++it) {
        if (it.value().name.contains(text, Qt::CaseInsensitive) || 
            it.value().lastMessage.contains(text, Qt::CaseInsensitive)) {
            filteredConversations[it.key()] = it.value();
        }
    }
    
    // 更新UI
    chatUIManager->loadConversations(filteredConversations);
}

void MainWindow::onContactSelected(const QModelIndex &index)
{
    // 获取用户ID
    QVariant userData = index.data(Qt::UserRole);
    if (userData.isValid()) {
        uint64_t uid = userData.toULongLong();
        
        // 获取联系人信息
        auto contactManager = ContactManager::getInstance();
        Contact contact;
        if (contactManager->getContactInfo(uid, contact)) {
            // 开始聊天
            auto chatUIManager = ChatUIManager::getInstance();
            chatUIManager->startNewChat(uid, contact.name, contact.avatarPath);
            
            // 切换到聊天页面
            onChatNavButtonClicked();
        }
    }
}

void MainWindow::onAddContactClicked()
{
    // 创建或显示添加联系人对话框
    if (!m_addContactDialog) {
        m_addContactDialog = new AddContactDialog(this);
        m_addContactDialog->setCurrentUser(m_clientInfo.uid, m_clientInfo.token);
        
        // 连接信号：当成功接受联系人请求时自动刷新主窗口的联系人列表
        connect(m_addContactDialog, &AddContactDialog::contactRequestAccepted, [this]() {
            if (m_isConnected && m_clientInfo.uid != 0) {
                auto contactManager = ContactManager::getInstance();
                contactManager->getContacts(m_clientInfo.uid, m_clientInfo.token);
            }
        });
    }
    
    // 刷新数据并显示对话框
    m_addContactDialog->refreshData();
    m_addContactDialog->show();
    m_addContactDialog->raise();
    m_addContactDialog->activateWindow();
}

void MainWindow::onContactContextMenu(const QPoint &pos)
{
    QModelIndex index = ui->contactsTreeView->indexAt(pos);
    if (index.isValid() && index.data(Qt::UserRole).isValid()) {
        uint64_t uid = index.data(Qt::UserRole).toULongLong();
        
        // 获取联系人信息
        auto contactManager = ContactManager::getInstance();
        Contact contact;
        if (!contactManager->getContactInfo(uid, contact)) {
            return;
        }
        
        QMenu contextMenu(this);
        QAction *chatAction = contextMenu.addAction("发起聊天");
        QAction *profileAction = contextMenu.addAction("查看资料");
        
        QAction *selectedAction = contextMenu.exec(ui->contactsTreeView->viewport()->mapToGlobal(pos));
        
        if (selectedAction == chatAction) {
            // 开始聊天
            auto chatUIManager = ChatUIManager::getInstance();
            chatUIManager->startNewChat(uid, contact.name, contact.avatarPath);
            
            // 切换到聊天页面
            onChatNavButtonClicked();
        } else if (selectedAction == profileAction) {
            // 显示联系人信息
            QString info = QString("用户名: %1\n状态: %2\n签名: %3")
                           .arg(contact.name)
                           .arg(contact.status)
                           .arg(contact.signature.isEmpty() ? "无" : contact.signature);
            QMessageBox::information(this, "联系人资料", info);
        }
    }
}

void MainWindow::onSearchContacts(const QString &text)
{
    // 实现联系人搜索
    if (text.isEmpty()) {
        // 显示所有联系人
        updateContactsView();
        return;
    }
    
    // 筛选匹配的联系人
    auto contactManager = ContactManager::getInstance();
    const auto &allContacts = contactManager->getAllContacts();
    
    // 清空当前模型
    m_contactsModel->clear();
    m_contactsModel->setHorizontalHeaderLabels(QStringList() << "联系人");
    
    QStandardItem *searchResults = new QStandardItem("搜索结果");
    searchResults->setEditable(false);
    m_contactsModel->appendRow(searchResults);
    
    // 添加匹配的联系人
    for (auto it = allContacts.begin(); it != allContacts.end(); ++it) {
        if (it.value().name.contains(text, Qt::CaseInsensitive) || 
            it.value().status.contains(text, Qt::CaseInsensitive) ||
            it.value().signature.contains(text, Qt::CaseInsensitive)) {
            QStandardItem *item = contactManager->createContactItem(it.value());
            searchResults->appendRow(item);
        }
    }
    
    // 展开搜索结果
    ui->contactsTreeView->expandAll();
}

void MainWindow::onSearchGroups(const QString &text)
{
    // TODO: 实现群组搜索
}

// 系统托盘功能
void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
            if (isHidden()) {
                show();
                activateWindow();
                raise();
            } else {
                hide();
            }
            break;
        default:
            break;
    }
}

void MainWindow::onTrayActionTriggered()
{
    show();
    activateWindow();
    raise();
}

// 网络事件处理
void MainWindow::onMessageReceived(uint64_t fromUid, const QString &content, const QString &msgType, qint64 timestamp)
{
    auto contactManager = ContactManager::getInstance();
    Contact contact;
    contactManager->getContactInfo(fromUid, contact); // 获取联系人信息以获得头像路径

    // 创建新消息
    ChatMessage chatMessage;
    chatMessage.senderId = fromUid;
    chatMessage.receiverId = m_clientInfo.uid;
    chatMessage.content = content;
    chatMessage.type = msgType;
    chatMessage.timestamp = timestamp;
    chatMessage.isOutgoing = false;
    chatMessage.avatarPath = contact.avatar_url; // 对方头像，使用正确的字段名
    
    // 添加消息到UI
    auto chatUIManager = ChatUIManager::getInstance();
    chatUIManager->addMessage(fromUid, chatMessage);
    
    // 显示通知
    QString senderName;
    if (!contact.name.isEmpty()) {
        senderName = contact.name;
    } else {
        senderName = QString("用户%1").arg(fromUid);
    }
    showNotification(senderName, content);
}

void MainWindow::onMessageSent(uint64_t toUid, const QString &content, bool success, const QString &message)
{
    if (!success) {
        // 显示发送失败的提示
        QMessageBox::warning(this, "消息发送失败", QString("消息发送失败: %1").arg(message));
    }
}

void MainWindow::onNetworkError(const QString &errorMessage)
{
    // 显示网络错误
    QMessageBox::warning(this, "网络错误", errorMessage);
}

// 辅助方法
void MainWindow::updateContactsView()
{
    // 清空当前模型
    m_contactsModel->clear();
    m_contactsModel->setHorizontalHeaderLabels(QStringList() << "联系人");
    
    QStandardItem *onlineGroup = new QStandardItem("在线");
    onlineGroup->setEditable(false);
    m_contactsModel->appendRow(onlineGroup);
    
    QStandardItem *offlineGroup = new QStandardItem("离线");
    offlineGroup->setEditable(false);
    m_contactsModel->appendRow(offlineGroup);
    
    // 获取所有联系人
    auto contactManager = ContactManager::getInstance();
    const auto &contacts = contactManager->getAllContacts();
    
    // 添加联系人
    for (auto it = contacts.begin(); it != contacts.end(); ++it) {
        QStandardItem *item = contactManager->createContactItem(it.value());
        
        if (it.value().isOnline) {
            onlineGroup->appendRow(item);
        } else {
            offlineGroup->appendRow(item);
        }
    }
    
    // 展开所有分组
    ui->contactsTreeView->expandAll();
}

void MainWindow::showNotification(const QString &title, const QString &message)
{
    if (m_trayIcon) {
        m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 3000);
    }
}

void MainWindow::updateConnectionStatus(bool isConnected, const QString &statusMessage)
{
    m_isConnected = isConnected;
    
    // 更新UI显示连接状态
    QString statusText = isConnected ? "已连接" : (statusMessage.isEmpty() ? "未连接" : statusMessage);
    statusBar()->showMessage(statusText);
    
    // 可以在这里更新其他UI元素以反映连接状态
}

void MainWindow::onUnreadCountChanged(uint64_t uid, int count)
{
    // 实现未读消息数量变化的逻辑
    qDebug() << "MainWindow: 会话" << uid << "未读消息数变为" << count;
    
    // 可以在这里更新UI，比如在聊天按钮上显示未读数
}

void MainWindow::onTotalUnreadCountChanged(int totalCount)
{
    // 实现总未读消息数量变化的逻辑
    qDebug() << "MainWindow: 总未读消息数变为" << totalCount;
    
    // 更新窗口标题显示未读数
    if (totalCount > 0) {
        setWindowTitle(QString("UChat (%1)").arg(totalCount));
        
        // 更新托盘图标提示
        if (m_trayIcon) {
            m_trayIcon->setToolTip(QString("UChat - %1条未读消息").arg(totalCount));
        }
    } else {
        setWindowTitle("UChat");
        if (m_trayIcon) {
            m_trayIcon->setToolTip("UChat");
        }
    }
}

void MainWindow::onNewMessageReceived(uint64_t fromUid, const QString &content)
{
    // 实现新消息接收的逻辑
    qDebug() << "MainWindow: 收到来自" << fromUid << "的新消息:" << content;
    
    // 如果窗口最小化或隐藏，闪烁任务栏图标
    if (isMinimized() || isHidden()) {
        QApplication::alert(this);
    }
}