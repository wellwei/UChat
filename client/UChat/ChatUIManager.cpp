#include "ChatUIManager.h"
#include <QTextBrowser>
#include <QListWidget>
#include <QLabel>
#include <QDateTime>
#include <QScrollBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QDebug>
#include "ContactManager.h"

ChatUIManager::ChatUIManager(QObject *parent) : QObject(parent),
    m_conversationsList(nullptr),
    m_messageBrowser(nullptr),
    m_chatTitleLabel(nullptr),
    m_currentChatUid(0),
    m_currentUserId(0)
{
}

ChatUIManager::~ChatUIManager()
{
}

void ChatUIManager::initUI(QListWidget *conversationsList, QTextBrowser *messageBrowser, QLabel *chatTitleLabel)
{
    m_conversationsList = conversationsList;
    m_messageBrowser = messageBrowser;
    m_chatTitleLabel = chatTitleLabel;
    
    // 连接信号和槽
    if (m_conversationsList) {
        connect(m_conversationsList, &QListWidget::itemClicked, this, &ChatUIManager::onConversationSelected);
    }
}

void ChatUIManager::loadConversations(const QMap<uint64_t, Conversation> &conversations)
{
    m_conversations = conversations;
    updateConversationList();
}

void ChatUIManager::updateConversationList()
{
    if (!m_conversationsList) return;
    
    m_conversationsList->clear();
    
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

void ChatUIManager::createConversationItem(const Conversation &conversation)
{
    if (!m_conversationsList) return;
    
    QListWidgetItem *item = new QListWidgetItem(m_conversationsList);
    item->setData(Qt::UserRole, QVariant::fromValue(conversation.uid));
    
    // 创建包含头像、名称、最后消息和时间的复杂项目
    QWidget *itemWidget = new QWidget(m_conversationsList);
    QHBoxLayout *layout = new QHBoxLayout(itemWidget);
    
    // 头像
    QLabel *avatarLabel = new QLabel(itemWidget);
    avatarLabel->setFixedSize(40, 40);
    QPixmap avatarPixmap = ContactManager::createAvatarPixmap(conversation.avatarPath);
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
    m_conversationsList->setItemWidget(item, itemWidget);
}

void ChatUIManager::loadChatHistory(uint64_t uid)
{
    if (!m_messageBrowser) return;
    
    // 清空消息显示区域
    m_messageBrowser->clear();
    
    if (m_conversations.contains(uid)) {
        // 获取聊天历史记录
        // 实际项目中这里会从服务器请求历史消息
        
        // 如果本地已有消息，显示它们
        const QList<ChatMessage> &messages = m_conversations[uid].messages;
        for (const ChatMessage &msg : messages) {
            displayMessage(msg);
        }
    }
    
    // 滚动到底部
    scrollToBottom();
}

void ChatUIManager::displayMessage(const ChatMessage &message)
{
    addMessageToBrowser(message);
    emit messageDisplayed();
}

void ChatUIManager::addMessage(uint64_t chatUid, const ChatMessage &message)
{
    if (m_conversations.contains(chatUid)) {
        // 添加消息到会话
        m_conversations[chatUid].messages.append(message);
        m_conversations[chatUid].lastMessage = message.content;
        m_conversations[chatUid].lastMessageTime = message.timestamp;
        
        // 如果不是当前会话，增加未读计数
        if (chatUid != m_currentChatUid && !message.isOutgoing) {
            m_conversations[chatUid].unreadCount++;
        }
        
        // 更新UI
        updateConversationList();
        
        // 如果是当前聊天对象，显示消息
        if (chatUid == m_currentChatUid) {
            displayMessage(message);
        }
    }
}

uint64_t ChatUIManager::getCurrentChatUid() const
{
    return m_currentChatUid;
}

void ChatUIManager::setCurrentChatUid(uint64_t uid)
{
    m_currentChatUid = uid;
    
    // 更新标题
    if (m_chatTitleLabel && m_conversations.contains(uid)) {
        m_chatTitleLabel->setText(m_conversations[uid].name);
    }
}

void ChatUIManager::setCurrentUserInfo(uint64_t userId, const QString &userName)
{
    m_currentUserId = userId;
    m_currentUserName = userName;
}

QMap<uint64_t, Conversation>& ChatUIManager::getConversations()
{
    return m_conversations;
}

void ChatUIManager::startNewChat(uint64_t uid, const QString &name, const QString &avatarPath)
{
    // 检查是否已有会话
    if (!m_conversations.contains(uid)) {
        // 创建新会话
        Conversation newConversation;
        newConversation.uid = uid;
        newConversation.name = name;
        newConversation.avatarPath = avatarPath;
        newConversation.lastMessageTime = QDateTime::currentDateTime().toSecsSinceEpoch();
        newConversation.unreadCount = 0;
        m_conversations[uid] = newConversation;
        
        // 更新会话列表
        updateConversationList();
    }
    
    // 设置当前会话
    setCurrentChatUid(uid);
    
    // 加载聊天历史
    loadChatHistory(uid);
    
    // 在列表中选中该会话
    if (m_conversationsList) {
        for (int i = 0; i < m_conversationsList->count(); ++i) {
            QListWidgetItem *item = m_conversationsList->item(i);
            if (item->data(Qt::UserRole).toULongLong() == uid) {
                m_conversationsList->setCurrentItem(item);
                break;
            }
        }
    }
    
    // 发出信号
    emit conversationSelected(uid);
}

void ChatUIManager::onConversationSelected(QListWidgetItem *item)
{
    if (!item) return;
    
    uint64_t uid = item->data(Qt::UserRole).toULongLong();
    if (m_conversations.contains(uid)) {
        // 更新当前聊天对象
        setCurrentChatUid(uid);
        
        // 重置未读消息计数
        m_conversations[uid].unreadCount = 0;
        
        // 更新会话列表项以反映未读数的变化
        updateConversationList();
        
        // 加载聊天历史
        loadChatHistory(uid);
        
        // 发出信号
        emit conversationSelected(uid);
    }
}

void ChatUIManager::addMessageToBrowser(const ChatMessage &message)
{
    if (!m_messageBrowser) return;
    
    QTextCursor cursor(m_messageBrowser->document());
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
        auto contactManager = ContactManager::getInstance();
        Contact contact;
        if (contactManager->getContactInfo(message.senderId, contact)) {
            senderName = contact.name;
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

void ChatUIManager::scrollToBottom()
{
    if (!m_messageBrowser) return;
    
    QScrollBar *scrollBar = m_messageBrowser->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

QString ChatUIManager::formatMessageTime(qint64 timestamp)
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