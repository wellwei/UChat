#include "ChatUIManager.h"
#include "ContactManager.h"
#include "MessageDelegate.h"
#include <QDebug>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScrollBar>
#include <QVBoxLayout>

ChatUIManager::ChatUIManager(QObject *parent) : QObject(parent),
                                                m_conversationsList(nullptr),
                                                m_messageListView(nullptr),
                                                m_chatTitleLabel(nullptr),
                                                m_chatModel(new ChatMessageModel(this)),
                                                m_currentChatUid(0),
                                                m_currentUserId(0)
{
}

ChatUIManager::~ChatUIManager()
{
}

void ChatUIManager::initUI(QListWidget *conversationsList, QListView *messageListView, QLabel *chatTitleLabel)
{
    m_conversationsList = conversationsList;
    m_messageListView = messageListView;
    m_chatTitleLabel = chatTitleLabel;

    if (m_conversationsList) {
        connect(m_conversationsList, &QListWidget::itemClicked, this, &ChatUIManager::onConversationSelected);
        m_conversationsList->setUniformItemSizes(false);
    }

    if (m_messageListView) {
        m_messageListView->setModel(m_chatModel);
        m_messageListView->setItemDelegate(new MessageDelegate(this));
        
        // 优化ListView样式和行为
        m_messageListView->setSelectionMode(QAbstractItemView::NoSelection);
        m_messageListView->setFocusPolicy(Qt::NoFocus);
        m_messageListView->setFrameShape(QFrame::NoFrame);
        m_messageListView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_messageListView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_messageListView->setResizeMode(QListView::Adjust);
        m_messageListView->setUniformItemSizes(false); 
        m_messageListView->setAutoScroll(true);
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

    auto item = new QListWidgetItem(m_conversationsList);
    item->setData(Qt::UserRole, QVariant::fromValue(conversation.uid));
    
    // 创建包含头像、名称、最后消息和时间的复杂项目
    auto itemWidget = new QWidget();
    const auto layout = new QHBoxLayout(itemWidget);
    
    // 头像
    const auto avatarLabel = new QLabel(itemWidget);
    avatarLabel->setFixedSize(40, 40);
    const QPixmap avatarPixmap = ContactManager::createAvatarPixmap(conversation.avatarPath);
    avatarLabel->setPixmap(avatarPixmap);
    avatarLabel->setScaledContents(true);
    
    // 名称、消息和时间的垂直布局
    const auto textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    
    // 第一行：名称和时间
    const auto nameTimeLayout = new QHBoxLayout();
    const auto nameLabel = new QLabel(conversation.name, itemWidget);
    nameLabel->setStyleSheet("font-weight: bold;");

    const auto timeLabel = new QLabel(formatMessageTime(conversation.lastMessageTime), itemWidget);
    timeLabel->setStyleSheet("color: gray; font-size: 9pt;");
    timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    nameTimeLayout->addWidget(nameLabel);
    nameTimeLayout->addWidget(timeLabel);
    
    // 第二行：最后消息和未读数
    const auto msgCountLayout = new QHBoxLayout();
    const auto msgLabel = new QLabel(conversation.lastMessage, itemWidget);
    msgLabel->setStyleSheet("color: gray; font-size: 9pt;");
    msgLabel->setMaximumWidth(200);
    
    // 文本省略
    const QString elidedText = msgLabel->fontMetrics().elidedText(
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
    if (!m_messageListView) return;
    
    m_chatModel->clearMessages();
    
    if (m_messageHistories.contains(uid)) {
        const QList<ChatMessage> &messages = m_messageHistories.value(uid);
        for (const ChatMessage &msg : messages) {
            m_chatModel->addMessage(msg);
        }
    }
    
    scrollToBottom();
}

void ChatUIManager::addMessage(uint64_t chatUid, const ChatMessage &message)
{
    // 存储历史记录
    m_messageHistories[chatUid].append(message);
    
    // 确保会话存在，如果不存在则创建
    if (!m_conversations.contains(chatUid)) {
        // 尝试从ContactManager获取联系人信息
        auto contactManager = ContactManager::getInstance();
        Contact contact;
        QString name;
        QString avatarPath;
        
        if (contactManager->getContactInfo(chatUid, contact)) {
            name = contact.name;
            avatarPath = contact.avatar_url;
        } else {
            // 如果没有联系人信息，使用默认名称
            name = QString("用户%1").arg(chatUid);
            avatarPath = QString(); // 使用默认头像
        }
        
        // 创建新会话
        createOrGetConversation(chatUid, name, avatarPath);
    }
    
    // 更新会话信息
    m_conversations[chatUid].lastMessage = message.content;
    m_conversations[chatUid].lastMessageTime = message.timestamp;
    
    // 如果不是当前聊天且是接收消息，增加未读计数
    if (chatUid != m_currentChatUid && !message.isOutgoing) {
        m_conversations[chatUid].unreadCount++;
        
        // 发出新消息信号
        emit newMessageReceived(chatUid, message.content);
        emit unreadCountChanged(chatUid, m_conversations[chatUid].unreadCount);
        emit totalUnreadCountChanged(getTotalUnreadCount());
    }
    
    // 更新会话列表（重新排序，显示未读数等）
    updateConversationList();
    
    // 如果是当前聊天，添加到消息模型并滚动到底部
    if (chatUid == m_currentChatUid) {
        m_chatModel->addMessage(message);
        scrollToBottom();
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
        
        // 标记为已读（使用新方法）
        markAsRead(uid);
        
        // 加载聊天历史
        loadChatHistory(uid);
        
        // 发出信号
        emit conversationSelected(uid);
    }
}

void ChatUIManager::scrollToBottom()
{
    if (!m_messageListView) return;
    
    m_messageListView->scrollToBottom();
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

void ChatUIManager::createOrGetConversation(uint64_t uid, const QString &name, const QString &avatarPath)
{
    // 如果会话已存在，直接返回
    if (m_conversations.contains(uid)) {
        return;
    }
    
    // 创建新会话
    Conversation newConversation;
    newConversation.uid = uid;
    newConversation.name = name.isEmpty() ? QString("用户%1").arg(uid) : name;
    newConversation.avatarPath = avatarPath;
    newConversation.lastMessage = QString(); // 初始为空
    newConversation.lastMessageTime = QDateTime::currentDateTime().toSecsSinceEpoch();
    newConversation.unreadCount = 0;
    
    // 添加到会话映射
    m_conversations[uid] = newConversation;
    
    qDebug() << "ChatUIManager: 创建新会话 - UID:" << uid << ", 名称:" << newConversation.name;
}

int ChatUIManager::getTotalUnreadCount() const
{
    int totalCount = 0;
    for (auto it = m_conversations.begin(); it != m_conversations.end(); ++it) {
        totalCount += it.value().unreadCount;
    }
    return totalCount;
}

int ChatUIManager::getUnreadCount(uint64_t uid) const
{
    if (m_conversations.contains(uid)) {
        return m_conversations[uid].unreadCount;
    }
    return 0;
}

void ChatUIManager::markAsRead(uint64_t uid)
{
    if (m_conversations.contains(uid) && m_conversations[uid].unreadCount > 0) {
        int oldCount = m_conversations[uid].unreadCount;
        m_conversations[uid].unreadCount = 0;
        
        // 更新会话列表显示
        updateConversationList();
        
        // 发出信号
        emit unreadCountChanged(uid, 0);
        emit totalUnreadCountChanged(getTotalUnreadCount());
        
        qDebug() << "ChatUIManager: 标记会话为已读 - UID:" << uid << ", 之前未读数:" << oldCount;
    }
}

void ChatUIManager::markAllAsRead()
{
    bool hasUnread = false;
    for (auto it = m_conversations.begin(); it != m_conversations.end(); ++it) {
        if (it.value().unreadCount > 0) {
            it.value().unreadCount = 0;
            hasUnread = true;
            emit unreadCountChanged(it.key(), 0);
        }
    }
    
    if (hasUnread) {
        updateConversationList();
        emit totalUnreadCountChanged(0);
        qDebug() << "ChatUIManager: 标记所有会话为已读";
    }
} 