#include <QFile>
#include <QDateTime>
#include <QCloseEvent>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QIcon>
#include <QInputDialog>
#include <QFileDialog>
#include <QDebug>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QTextDocument>
#include <QScrollBar>
#include <QStandardItem>
#include <QRandomGenerator>
#include <QKeyEvent>

#include "mainwindow.hpp"
#include "./ui_mainwindow.h"
#include "logindialog.hpp"
#include "HttpMgr.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_currentChatUid(0)
    , m_currentUserId(0)
    , m_isConnected(false)
    , m_contactsModel(nullptr)
    , m_chatConnection(nullptr)
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
    initModels();
    setupSignalsAndSlots();
    
    // 默认显示聊天页面
    ui->centralStackedWidget->setCurrentIndex(0);
    ui->chatNavButton->setChecked(true);
    
    // 添加事件过滤器用于处理特定的键盘事件
    ui->messageInputEdit->installEventFilter(this);
}

MainWindow::~MainWindow()
{
    if (m_chatConnection) {
        m_chatConnection->disconnect();
        delete m_chatConnection;
    }
    
    delete ui;
}

void MainWindow::initUI()
{
    // 设置窗口标题和图标
    setWindowTitle("UChat");
    setWindowIcon(QIcon(":/resource/image/uchat.ico"));
    
    // 设置侧边栏图标
    ui->chatNavButton->setIcon(QIcon(":/resource/image/chat-message.svg"));
    ui->contactsNavButton->setIcon(QIcon(":/resource/image/contacter.svg"));
    ui->settingsNavButton->setIcon(QIcon(":/resource/image/setting.svg"));
    
    // 确保分割器的初始位置合理
    ui->chatSplitter->setSizes(QList<int>() << 250 << 550);
    
    // 初始化会话列表为空
    ui->conversationsListWidget->clear();
    
    // 初始化消息显示区域
    ui->messageDisplayBrowser->clear();
    ui->messageDisplayBrowser->setOpenLinks(true);
    ui->messageDisplayBrowser->setOpenExternalLinks(true);
    
    // 设置输入区域初始状态
    ui->messageInputEdit->clear();
    ui->messageInputEdit->setFocus();
}

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

void MainWindow::initModels()
{
    // 初始化联系人模型
    m_contactsModel = new QStandardItemModel(this);
    ui->contactsTreeView->setModel(m_contactsModel);
    
    // 设置联系人模型的列
    m_contactsModel->setHorizontalHeaderLabels(QStringList() << "联系人");
}

void MainWindow::initChatConnection()
{
    if (m_chatConnection) {
        delete m_chatConnection;
    }
    
    m_chatConnection = new ChatConnection(this);
    
    // 连接信号槽
    connect(m_chatConnection, &ChatConnection::stateChanged, 
            this, &MainWindow::onConnectionStateChanged);
    connect(m_chatConnection, &ChatConnection::messageReceived, 
            this, &MainWindow::onMessageReceived);
    connect(m_chatConnection, &ChatConnection::messageSent, 
            this, &MainWindow::onMessageSent);
    connect(m_chatConnection, &ChatConnection::error, 
            this, &MainWindow::onNetworkError);
    
    // 连接到服务器
    if (!m_serverInfo.host.isEmpty()) {
        QStringList hostParts = m_serverInfo.host.split(":");
        if (hostParts.size() == 2) {
            QString host = hostParts.at(0);
            quint16 port = hostParts.at(1).toUInt();
            m_chatConnection->connectToServer(host, port);
        }
    }
}

void MainWindow::showLoginDialog()
{
    LoginDialog dialog(this);
    // 直接使用C++11的lambda表达式来连接信号
    connect(&dialog, &LoginDialog::loginSuccess, 
            [this](const ServerInfo &serverInfo) {
                this->onLoginSuccess(serverInfo);
            });
    
    // 对话框关闭时，如果仍未登陆，则关闭程序
    if (dialog.exec() != QDialog::Accepted) {
        // 检查是否已登录，如果没有则退出应用
        if (m_currentUserId == 0) {
            QApplication::quit();
        }
    }
}

void MainWindow::onLoginSuccess(const ServerInfo &serverInfo)
{
    // 保存服务器信息
    m_serverInfo = serverInfo;
    m_currentUserId = serverInfo.uid;
    
    // TODO: 从服务器获取用户信息
    m_currentUserName = QString("用户%1").arg(serverInfo.uid);
    m_currentUserAvatar = ""; // 默认头像
    
    // 初始化聊天连接
    initChatConnection();
    
    // 加载会话和联系人数据
    loadConversations();
    loadContacts();
    
    // 更新UI
    setWindowTitle(QString("UChat - %1").arg(m_currentUserName));
    show();
}

void MainWindow::setupSignalsAndSlots()
{
    // 连接导航按钮信号
    connect(ui->chatNavButton, &QToolButton::clicked, this, &MainWindow::onChatNavButtonClicked);
    connect(ui->contactsNavButton, &QToolButton::clicked, this, &MainWindow::onContactsNavButtonClicked);
    connect(ui->groupsNavButton, &QToolButton::clicked, this, &MainWindow::onGroupsNavButtonClicked);
    connect(ui->settingsNavButton, &QToolButton::clicked, this, &MainWindow::onSettingsNavButtonClicked);
    
    // 连接会话列表信号
    connect(ui->conversationsListWidget, &QListWidget::itemClicked, this, &MainWindow::onConversationSelected);
    
    // 连接搜索框信号
    connect(ui->searchConversationsLineEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchConversations);
    connect(ui->searchContactsLineEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchContacts);
    connect(ui->searchGroupsLineEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchGroups);
    
    // 连接聊天功能信号
    connect(ui->sendButton, &QPushButton::clicked, this, &MainWindow::onSendButtonClicked);
    connect(ui->messageInputEdit, &QTextEdit::textChanged, this, &MainWindow::onMessageInputTextChanged);
    connect(ui->emojiButton, &QToolButton::clicked, this, &MainWindow::onEmojiButtonClicked);
    connect(ui->fileButton, &QToolButton::clicked, this, &MainWindow::onFileButtonClicked);
    connect(ui->screenshotButton, &QToolButton::clicked, this, &MainWindow::onScreenshotButtonClicked);
    
    // 连接联系人信号
    connect(ui->contactsTreeView, &QTreeView::clicked, this, &MainWindow::onContactSelected);
    connect(ui->contactsTreeView, &QTreeView::customContextMenuRequested, this, &MainWindow::onContactContextMenu);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->messageInputEdit) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Return && keyEvent->modifiers() == Qt::NoModifier) {
                // 按下回车键发送消息
                onSendButtonClicked();
                return true;
            } else if (keyEvent->key() == Qt::Key_Return && keyEvent->modifiers() == Qt::ShiftModifier) {
                // Shift+回车插入换行
                ui->messageInputEdit->insertPlainText("\n");
                return true;
            }
        }
    }
    
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_trayIcon && m_trayIcon->isVisible()) {
        // 隐藏到系统托盘而不是退出
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

// 导航按钮事件处理
void MainWindow::onChatNavButtonClicked()
{
    ui->centralStackedWidget->setCurrentIndex(0);
    
    // 更新按钮图标
    ui->chatNavButton->setIcon(QIcon(":/resource/image/chat-message-pressed.svg"));
    ui->contactsNavButton->setIcon(QIcon(":/resource/image/contacter.svg"));
    ui->settingsNavButton->setIcon(QIcon(":/resource/image/setting.svg"));
}

void MainWindow::onContactsNavButtonClicked()
{
    ui->centralStackedWidget->setCurrentIndex(1);
    
    // 更新按钮图标
    ui->chatNavButton->setIcon(QIcon(":/resource/image/chat-message.svg"));
    ui->contactsNavButton->setIcon(QIcon(":/resource/image/contacter-pressed.svg"));
    ui->settingsNavButton->setIcon(QIcon(":/resource/image/setting.svg"));
}

void MainWindow::onGroupsNavButtonClicked()
{
    ui->centralStackedWidget->setCurrentIndex(2);
    
    // 更新按钮图标
    ui->chatNavButton->setIcon(QIcon(":/resource/image/chat-message.svg"));
    ui->contactsNavButton->setIcon(QIcon(":/resource/image/contacter.svg"));
    ui->settingsNavButton->setIcon(QIcon(":/resource/image/setting.svg"));
}

void MainWindow::onSettingsNavButtonClicked()
{
    ui->centralStackedWidget->setCurrentIndex(3);
    
    // 更新按钮图标
    ui->chatNavButton->setIcon(QIcon(":/resource/image/chat-message.svg"));
    ui->contactsNavButton->setIcon(QIcon(":/resource/image/contacter.svg"));
    ui->settingsNavButton->setIcon(QIcon(":/resource/image/setting-pressed.svg"));
}

// 聊天相关功能实现
void MainWindow::loadConversations()
{
    // 清空当前会话列表
    ui->conversationsListWidget->clear();
    m_conversations.clear();
    
    // TODO: 从服务器获取会话列表
    // 暂时添加一些模拟数据用于开发
    
    // 添加一些测试会话
    Conversation conv1;
    conv1.uid = 10001;
    conv1.name = "张三";
    conv1.lastMessage = "下午有空吗？";
    conv1.lastMessageTime = QDateTime::currentDateTime().addSecs(-3600).toSecsSinceEpoch(); // 1小时前
    conv1.unreadCount = 2;
    m_conversations[conv1.uid] = conv1;
    
    Conversation conv2;
    conv2.uid = 10002;
    conv2.name = "李四";
    conv2.lastMessage = "文件我已经发送给你了";
    conv2.lastMessageTime = QDateTime::currentDateTime().addSecs(-7200).toSecsSinceEpoch(); // 2小时前
    conv2.unreadCount = 0;
    m_conversations[conv2.uid] = conv2;
    
    Conversation conv3;
    conv3.uid = 10003;
    conv3.name = "项目群";
    conv3.lastMessage = "王五: 明天记得开会";
    conv3.lastMessageTime = QDateTime::currentDateTime().addDays(-1).toSecsSinceEpoch(); // 昨天
    conv3.unreadCount = 5;
    m_conversations[conv3.uid] = conv3;
    
    // 更新UI
    updateConversationList();
}

void MainWindow::updateConversationList()
{
    ui->conversationsListWidget->clear();
    
    // 对会话按最后消息时间进行排序（最新的在前面）
    QList<uint64_t> sortedKeys;
    for (auto it = m_conversations.begin(); it != m_conversations.end(); ++it) {
        sortedKeys.append(it.key());
    }
    
    std::sort(sortedKeys.begin(), sortedKeys.end(), [this](uint64_t a, uint64_t b) {
        return m_conversations[a].lastMessageTime > m_conversations[b].lastMessageTime;
    });
    
    // 添加排序后的会话到列表
    for (uint64_t uid : sortedKeys) {
        createConversationItem(m_conversations[uid]);
    }
}

void MainWindow::createConversationItem(const Conversation &conversation)
{
    QListWidgetItem *item = new QListWidgetItem(ui->conversationsListWidget);
    item->setData(Qt::UserRole, QVariant::fromValue(conversation.uid));
    
    // 创建包含头像、名称、最后消息和时间的复杂项目
    QWidget *itemWidget = new QWidget(ui->conversationsListWidget);
    QHBoxLayout *layout = new QHBoxLayout(itemWidget);
    
    // 头像
    QLabel *avatarLabel = new QLabel(itemWidget);
    avatarLabel->setFixedSize(40, 40);
    QPixmap avatarPixmap = createAvatarPixmap(conversation.avatarPath);
    avatarLabel->setPixmap(avatarPixmap);
    avatarLabel->setScaledContents(true);
    
    // 名称、消息和时间的垂直布局
    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    
    // 第一行：名称和时间
    QHBoxLayout *nameTimeLayout = new QHBoxLayout();
    QLabel *nameLabel = new QLabel(conversation.name, itemWidget);
    nameLabel->setStyleSheet("font-weight: bold;");
    
    QLabel *timeLabel = new QLabel(formatMessageTime(conversation.lastMessageTime), itemWidget);
    timeLabel->setStyleSheet("color: gray; font-size: 9pt;");
    timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    nameTimeLayout->addWidget(nameLabel);
    nameTimeLayout->addWidget(timeLabel);
    
    // 第二行：最后消息和未读数
    QHBoxLayout *msgCountLayout = new QHBoxLayout();
    QLabel *msgLabel = new QLabel(conversation.lastMessage, itemWidget);
    msgLabel->setStyleSheet("color: gray; font-size: 9pt;");
    msgLabel->setMaximumWidth(200);
    
    // 文本省略
    QString elidedText = msgLabel->fontMetrics().elidedText(
        conversation.lastMessage, Qt::ElideRight, msgLabel->maximumWidth());
    msgLabel->setText(elidedText);
    
    QLabel *countLabel = nullptr;
    if (conversation.unreadCount > 0) {
        countLabel = new QLabel(QString::number(conversation.unreadCount), itemWidget);
        countLabel->setStyleSheet(
            "background-color: #FF3B30; color: white; border-radius: 9px; "
            "min-width: 18px; max-width: 18px; min-height: 18px; max-height: 18px; "
            "font-size: 8pt; font-weight: bold; padding: 0px;"
        );
        countLabel->setAlignment(Qt::AlignCenter);
    } else {
        countLabel = new QLabel(itemWidget);
        countLabel->setFixedSize(18, 18);
    }
    
    msgCountLayout->addWidget(msgLabel);
    msgCountLayout->addWidget(countLabel);
    
    // 添加两行到垂直布局
    textLayout->addLayout(nameTimeLayout);
    textLayout->addLayout(msgCountLayout);
    
    // 组装完整布局
    layout->addWidget(avatarLabel);
    layout->addLayout(textLayout, 1);
    layout->setContentsMargins(5, 5, 5, 5);
    itemWidget->setLayout(layout);
    
    // 设置项高度
    item->setSizeHint(itemWidget->sizeHint());
    
    // 将自定义小部件设置为项目
    ui->conversationsListWidget->setItemWidget(item, itemWidget);
}

void MainWindow::onConversationSelected(QListWidgetItem *item)
{
    if (!item) return;
    
    uint64_t uid = item->data(Qt::UserRole).toULongLong();
    if (m_conversations.contains(uid)) {
        // 更新当前聊天对象
        m_currentChatUid = uid;
        Conversation &conversation = m_conversations[uid];
        
        // 更新标题
        ui->chatTargetNameLabel->setText(conversation.name);
        
        // 重置未读消息计数
        conversation.unreadCount = 0;
        
        // 更新会话列表项以反映未读数的变化
        updateConversationList();
        
        // 加载聊天历史
        loadChatHistory(uid);
    }
}

void MainWindow::loadChatHistory(uint64_t uid)
{
    // 清空消息显示区域
    ui->messageDisplayBrowser->clear();
    
    if (m_conversations.contains(uid)) {
        // TODO: 从服务器获取完整聊天历史
        
        // 如果本地已有消息，显示它们
        const QList<ChatMessage> &messages = m_conversations[uid].messages;
        for (const ChatMessage &msg : messages) {
            displayMessage(msg);
        }
        
        // 如果没有本地消息，添加一些测试消息
        if (messages.isEmpty()) {
            // 添加一些模拟消息用于开发
            QList<ChatMessage> testMessages;
            
            ChatMessage msg1;
            msg1.senderId = m_currentUserId;
            msg1.receiverId = uid;
            msg1.content = "你好！";
            msg1.type = "text";
            msg1.timestamp = QDateTime::currentDateTime().addSecs(-3600).toSecsSinceEpoch();
            msg1.isOutgoing = true;
            testMessages.append(msg1);
            
            ChatMessage msg2;
            msg2.senderId = uid;
            msg2.receiverId = m_currentUserId;
            msg2.content = "你好，最近怎么样？";
            msg2.type = "text";
            msg2.timestamp = QDateTime::currentDateTime().addSecs(-3500).toSecsSinceEpoch();
            msg2.isOutgoing = false;
            testMessages.append(msg2);
            
            ChatMessage msg3;
            msg3.senderId = m_currentUserId;
            msg3.receiverId = uid;
            msg3.content = "一切都好，你呢？";
            msg3.type = "text";
            msg3.timestamp = QDateTime::currentDateTime().addSecs(-3400).toSecsSinceEpoch();
            msg3.isOutgoing = true;
            testMessages.append(msg3);
            
            // 显示测试消息
            for (const ChatMessage &msg : testMessages) {
                displayMessage(msg);
                m_conversations[uid].messages.append(msg);
            }
        }
    }
    
    // 滚动到底部
    scrollToBottom();
}

void MainWindow::displayMessage(const ChatMessage &message)
{
    addMessageToBrowser(message);
}

void MainWindow::addMessageToBrowser(const ChatMessage &message)
{
    QTextCursor cursor(ui->messageDisplayBrowser->document());
    cursor.movePosition(QTextCursor::End);
    
    // 在每条消息之间添加一些垂直空间
    QTextBlockFormat blockFormat;
    blockFormat.setTopMargin(8);
    cursor.insertBlock(blockFormat);
    
    // 确定消息发送者的名称和颜色
    QString senderName;
    QString bubbleStyle;
    Qt::Alignment alignment;
    
    if (message.isOutgoing) {
        senderName = m_currentUserName;
        alignment = Qt::AlignRight;
        bubbleStyle = "background-color: #DCF8C6; border-radius: 8px; padding: 8px; margin-left: 80px; margin-right: 10px;";
    } else {
        // 查找联系人名称
        if (m_contacts.contains(message.senderId)) {
            senderName = m_contacts[message.senderId].name;
        } else if (m_conversations.contains(message.senderId)) {
            senderName = m_conversations[message.senderId].name;
        } else {
            senderName = QString("用户%1").arg(message.senderId);
        }
        alignment = Qt::AlignLeft;
        bubbleStyle = "background-color: #FFFFFF; border-radius: 8px; padding: 8px; margin-right: 80px; margin-left: 10px;";
    }
    
    // 格式化时间
    QString timeStr = formatMessageTime(message.timestamp);
    
    // 创建RTF文档片段
    QString html = QString(
        "<div style='margin: 5px;'>"
        "   <div style='%1'>"
        "       <div style='font-size: 9pt; color: #888888;'>%2</div>"
        "       <div>%3</div>"
        "       <div style='font-size: 8pt; color: #888888; text-align: right;'>%4</div>"
        "   </div>"
        "</div>"
    ).arg(bubbleStyle, senderName, message.content, timeStr);
    
    // 插入HTML
    cursor.insertHtml(html);
    
    // 滚动到底部
    scrollToBottom();
}

void MainWindow::scrollToBottom()
{
    QScrollBar *scrollBar = ui->messageDisplayBrowser->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

QString MainWindow::formatMessageTime(qint64 timestamp)
{
    QDateTime messageTime = QDateTime::fromSecsSinceEpoch(timestamp);
    QDateTime now = QDateTime::currentDateTime();
    QDateTime today = QDateTime(now.date(), QTime(0, 0, 0));
    QDateTime yesterday = today.addDays(-1);
    
    if (messageTime >= today) {
        // 今天 - 显示时间
        return messageTime.toString("HH:mm");
    } else if (messageTime >= yesterday) {
        // 昨天 - 显示昨天和时间
        return QString("昨天 %1").arg(messageTime.toString("HH:mm"));
    } else if (messageTime.date().year() == now.date().year()) {
        // 今年 - 显示月日和时间
        return messageTime.toString("MM-dd HH:mm");
    } else {
        // 其他 - 显示完整日期和时间
        return messageTime.toString("yyyy-MM-dd HH:mm");
    }
}

void MainWindow::onSendButtonClicked()
{
    QString message = ui->messageInputEdit->toPlainText().trimmed();
    if (message.isEmpty() || m_currentChatUid == 0) {
        return; // 不发送空消息或无聊天对象
    }
    
    // 创建消息对象
    ChatMessage chatMessage;
    chatMessage.senderId = m_currentUserId;
    chatMessage.receiverId = m_currentChatUid;
    chatMessage.content = message;
    chatMessage.type = "text";
    chatMessage.timestamp = QDateTime::currentDateTime().toSecsSinceEpoch();
    chatMessage.isOutgoing = true;
    
    // 显示消息
    displayMessage(chatMessage);
    
    // 添加到会话消息历史
    if (m_conversations.contains(m_currentChatUid)) {
        m_conversations[m_currentChatUid].messages.append(chatMessage);
        m_conversations[m_currentChatUid].lastMessage = message;
        m_conversations[m_currentChatUid].lastMessageTime = chatMessage.timestamp;
        
        // 更新会话列表
        updateConversationList();
    }
    
    // 发送到服务器
    if (m_chatConnection && m_chatConnection->isAuthenticated()) {
        m_chatConnection->sendChatMessage(m_currentChatUid, message);
    }
    
    // 清空输入框
    ui->messageInputEdit->clear();
}

void MainWindow::onMessageInputTextChanged()
{
    // 可以在这里添加"正在输入"状态的处理
    QString text = ui->messageInputEdit->toPlainText();
    ui->sendButton->setEnabled(!text.trimmed().isEmpty());
    
    // 根据是否有内容改变发送按钮的样式
    if (!text.trimmed().isEmpty()) {
        ui->sendButton->setStyleSheet("background-color: #0078D4; color: white; border: none;");
    } else {
        ui->sendButton->setStyleSheet("");
    }
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
    if (text.isEmpty()) {
        // 显示所有会话
        updateConversationList();
        return;
    }
    
    // 筛选匹配的会话
    ui->conversationsListWidget->clear();
    
    for (auto it = m_conversations.begin(); it != m_conversations.end(); ++it) {
        if (it.value().name.contains(text, Qt::CaseInsensitive) || 
            it.value().lastMessage.contains(text, Qt::CaseInsensitive)) {
            createConversationItem(it.value());
        }
    }
}

// 消息处理
void MainWindow::onMessageReceived(uint64_t fromUid, const QString &content, const QString &msgType, qint64 timestamp)
{
    // 创建新消息
    ChatMessage chatMessage;
    chatMessage.senderId = fromUid;
    chatMessage.receiverId = m_currentUserId;
    chatMessage.content = content;
    chatMessage.type = msgType;
    chatMessage.timestamp = timestamp;
    chatMessage.isOutgoing = false;
    
    // 检查会话是否存在
    if (!m_conversations.contains(fromUid)) {
        // 创建新会话
        Conversation newConversation;
        newConversation.uid = fromUid;
        
        // 尝试从联系人获取名称
        if (m_contacts.contains(fromUid)) {
            newConversation.name = m_contacts[fromUid].name;
            newConversation.avatarPath = m_contacts[fromUid].avatarPath;
        } else {
            newConversation.name = QString("用户%1").arg(fromUid);
        }
        
        m_conversations[fromUid] = newConversation;
    }
    
    // 更新会话最后消息
    m_conversations[fromUid].lastMessage = content;
    m_conversations[fromUid].lastMessageTime = timestamp;
    m_conversations[fromUid].messages.append(chatMessage);
    
    // 如果不是当前会话，增加未读计数
    if (fromUid != m_currentChatUid) {
        m_conversations[fromUid].unreadCount++;
        
        // 显示通知
        showNotification(m_conversations[fromUid].name, content);
    } else {
        // 如果是当前会话，直接显示消息
        displayMessage(chatMessage);
    }
    
    // 更新会话列表
    updateConversationList();
}

void MainWindow::onMessageSent(uint64_t toUid, const QString &content, bool success, const QString &message)
{
    // 处理消息发送结果
    if (!success) {
        QMessageBox::warning(this, "消息发送失败", message);
        
        // TODO: 在UI中标记消息发送失败
    }
}

void MainWindow::showNotification(const QString &title, const QString &message)
{
    if (m_trayIcon && QSystemTrayIcon::supportsMessages()) {
        m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 3000);
    }
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        // 单击或双击托盘图标时显示窗口
        this->show();
        this->activateWindow();
    }
}

QPixmap MainWindow::createAvatarPixmap(const QString &avatarPath, bool isOnline)
{
    QPixmap avatarPixmap;
    
    // 尝试加载头像图片
    if (!avatarPath.isEmpty() && avatarPixmap.load(avatarPath)) {
        // 头像图片加载成功
    } else {
        // 创建默认头像
        avatarPixmap = QPixmap(40, 40);
        avatarPixmap.fill(Qt::lightGray);
        
        QPainter painter(&avatarPixmap);
        painter.setPen(Qt::NoPen);
        
        // 随机生成背景颜色 - 用QRandomGenerator替代qrand
        QColor bgColor = QColor::fromHsv(QRandomGenerator::global()->bounded(360), 200, 200);
        painter.setBrush(bgColor);
        painter.drawEllipse(avatarPixmap.rect());
        
        // 绘制首字母
        QString letter = "U";
        if (!m_currentUserName.isEmpty()) {
            letter = m_currentUserName.at(0).toUpper();
        }
        
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 16, QFont::Bold));
        painter.drawText(avatarPixmap.rect(), Qt::AlignCenter, letter);
        painter.end();
    }
    
    // 将头像做成圆形
    QPixmap roundedPixmap(avatarPixmap.size());
    roundedPixmap.fill(Qt::transparent);
    
    QPainter painter(&roundedPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QPainterPath path;
    path.addEllipse(roundedPixmap.rect());
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, avatarPixmap);
    
    // 如果需要，添加在线状态指示器
    if (isOnline) {
        painter.setBrush(Qt::green);
        painter.setPen(Qt::white);
        painter.drawEllipse(roundedPixmap.width() - 12, roundedPixmap.height() - 12, 10, 10);
    }
    
    painter.end();
    return roundedPixmap;
}

// 联系人管理功能
void MainWindow::loadContacts()
{
    // 清空当前联系人列表
    m_contacts.clear();
    
    // TODO: 从服务器获取联系人列表
    // 暂时添加一些模拟数据用于开发
    
    Contact contact1;
    contact1.uid = 10001;
    contact1.name = "张三";
    contact1.status = "在线";
    contact1.isOnline = true;
    m_contacts[contact1.uid] = contact1;
    
    Contact contact2;
    contact2.uid = 10002;
    contact2.name = "李四";
    contact2.status = "正在工作";
    contact2.isOnline = true;
    m_contacts[contact2.uid] = contact2;
    
    Contact contact3;
    contact3.uid = 10003;
    contact3.name = "王五";
    contact3.status = "离线";
    contact3.isOnline = false;
    m_contacts[contact3.uid] = contact3;
    
    Contact contact4;
    contact4.uid = 10004;
    contact4.name = "赵六";
    contact4.status = "忙碌中";
    contact4.isOnline = true;
    m_contacts[contact4.uid] = contact4;
    
    // 更新界面
    updateContactsModel();
}

void MainWindow::updateContactsModel()
{
    m_contactsModel->clear();
    m_contactsModel->setHorizontalHeaderLabels(QStringList() << "联系人");
    
    QStandardItem *onlineGroup = new QStandardItem("在线");
    onlineGroup->setEditable(false);
    m_contactsModel->appendRow(onlineGroup);
    
    QStandardItem *offlineGroup = new QStandardItem("离线");
    offlineGroup->setEditable(false);
    m_contactsModel->appendRow(offlineGroup);
    
    // 添加联系人
    for (auto it = m_contacts.begin(); it != m_contacts.end(); ++it) {
        QStandardItem *item = createContactItem(it.value());
        
        if (it.value().isOnline) {
            onlineGroup->appendRow(item);
        } else {
            offlineGroup->appendRow(item);
        }
    }
    
    // 展开所有分组
    ui->contactsTreeView->expandAll();
}

QStandardItem* MainWindow::createContactItem(const Contact &contact)
{
    QStandardItem *item = new QStandardItem();
    item->setData(QVariant::fromValue(contact.uid), Qt::UserRole);
    
    // 创建自定义小部件
    QWidget *widget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(2, 2, 2, 2);
    
    // 头像
    QLabel *avatarLabel = new QLabel();
    avatarLabel->setFixedSize(32, 32);
    QPixmap avatarPixmap = createAvatarPixmap(contact.avatarPath, contact.isOnline);
    avatarLabel->setPixmap(avatarPixmap);
    
    // 名称和状态
    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(0);
    
    QLabel *nameLabel = new QLabel(contact.name);
    nameLabel->setStyleSheet("font-weight: bold;");
    
    QLabel *statusLabel = new QLabel(contact.status);
    statusLabel->setStyleSheet("color: gray; font-size: 9pt;");
    
    textLayout->addWidget(nameLabel);
    textLayout->addWidget(statusLabel);
    
    layout->addWidget(avatarLabel);
    layout->addLayout(textLayout, 1);
    
    // 设置项目文本，但它会被自定义小部件覆盖
    item->setText(contact.name);
    item->setEditable(false);
    
    return item;
}

void MainWindow::onContactSelected(const QModelIndex &index)
{
    // 获取用户ID
    QVariant userData = index.data(Qt::UserRole);
    if (userData.isValid()) {
        uint64_t uid = userData.toULongLong();
        
        // 开始与该联系人的聊天
        if (m_contacts.contains(uid)) {
            startNewChat(uid, m_contacts[uid].name);
        }
    }
}

void MainWindow::startNewChat(uint64_t uid, const QString &name)
{
    // 检查是否已有会话
    if (!m_conversations.contains(uid)) {
        // 创建新会话
        Conversation newConversation;
        newConversation.uid = uid;
        newConversation.name = name;
        newConversation.lastMessageTime = QDateTime::currentDateTime().toSecsSinceEpoch();
        newConversation.unreadCount = 0;
        m_conversations[uid] = newConversation;
        
        // 更新会话列表
        updateConversationList();
    }
    
    // 切换到聊天页面
    onChatNavButtonClicked();
    
    // 选择会话
    for (int i = 0; i < ui->conversationsListWidget->count(); ++i) {
        QListWidgetItem *item = ui->conversationsListWidget->item(i);
        if (item->data(Qt::UserRole).toULongLong() == uid) {
            ui->conversationsListWidget->setCurrentItem(item);
            onConversationSelected(item);
            break;
        }
    }
}

void MainWindow::onAddContactClicked()
{
    // 弹出添加联系人对话框
    bool ok;
    QString userId = QInputDialog::getText(this, "添加联系人", 
                                         "请输入用户ID或邮箱:", 
                                         QLineEdit::Normal, 
                                         "", &ok);
    if (ok && !userId.isEmpty()) {
        // TODO: 向服务器发送添加联系人请求
        QMessageBox::information(this, "添加联系人", 
                               QString("添加联系人 %1 的请求已发送").arg(userId));
    }
}

void MainWindow::onContactContextMenu(const QPoint &pos)
{
    QModelIndex index = ui->contactsTreeView->indexAt(pos);
    if (index.isValid() && index.data(Qt::UserRole).isValid()) {
        uint64_t uid = index.data(Qt::UserRole).toULongLong();
        
        QMenu contextMenu(this);
        QAction *chatAction = contextMenu.addAction("发起聊天");
        QAction *profileAction = contextMenu.addAction("查看资料");
        QAction *deleteAction = contextMenu.addAction("删除联系人");
        
        QAction *selectedAction = contextMenu.exec(ui->contactsTreeView->viewport()->mapToGlobal(pos));
        
        if (selectedAction == chatAction) {
            startNewChat(uid, m_contacts[uid].name);
        } else if (selectedAction == profileAction) {
            // TODO: 显示联系人资料
            QMessageBox::information(this, "联系人资料", 
                                  QString("查看联系人 %1 的资料").arg(m_contacts[uid].name));
        } else if (selectedAction == deleteAction) {
            // 确认删除
            int result = QMessageBox::question(this, "删除联系人", 
                                            QString("确定要删除联系人 %1 吗?").arg(m_contacts[uid].name), 
                                            QMessageBox::Yes | QMessageBox::No);
            
            if (result == QMessageBox::Yes) {
                // TODO: 向服务器发送删除联系人请求
                m_contacts.remove(uid);
                updateContactsModel();
                QMessageBox::information(this, "删除联系人", "联系人已删除");
            }
        }
    }
}

void MainWindow::onSearchContacts(const QString &text)
{
    if (text.isEmpty()) {
        // 恢复完整联系人列表
        updateContactsModel();
        return;
    }
    
    // 筛选匹配的联系人
    m_contactsModel->clear();
    m_contactsModel->setHorizontalHeaderLabels(QStringList() << "搜索结果");
    
    QStandardItem *resultGroup = new QStandardItem("匹配结果");
    resultGroup->setEditable(false);
    m_contactsModel->appendRow(resultGroup);
    
    for (auto it = m_contacts.begin(); it != m_contacts.end(); ++it) {
        if (it.value().name.contains(text, Qt::CaseInsensitive) || 
            it.value().status.contains(text, Qt::CaseInsensitive)) {
            QStandardItem *item = createContactItem(it.value());
            resultGroup->appendRow(item);
        }
    }
    
    ui->contactsTreeView->expandAll();
}

void MainWindow::onSearchGroups(const QString &text)
{
    // TODO: 实现群组搜索
    // 这里暂时不做具体实现，因为没有群组数据
}

// 网络连接相关功能
void MainWindow::onConnectionStateChanged(ChatConnection::ConnectionState state)
{
    switch (state) {
        case ChatConnection::Connected:
            m_isConnected = true;
            // 认证用户
            m_chatConnection->authenticate(m_currentUserId, m_serverInfo.token);
            break;
            
        case ChatConnection::Authenticated:
            // 认证成功，可以开始发送消息
            statusBar()->showMessage("已连接到服务器", 3000);
            break;
            
        case ChatConnection::Disconnected:
        case ChatConnection::Error:
            m_isConnected = false;
            statusBar()->showMessage("与服务器的连接已断开", 3000);
            // 可以在这里添加重连逻辑
            break;
            
        default:
            break;
    }
}

void MainWindow::onNetworkError(const QString &errorMessage)
{
    statusBar()->showMessage(QString("网络错误: %1").arg(errorMessage), 5000);
    // 可以在这里添加错误处理逻辑
}

void MainWindow::onTrayActionTriggered()
{
    if (isVisible()) {
        hide();
    } else {
        show();
        activateWindow();
    }
}