#include "AddContactDialog.h"
#include "ContactManager.h"
#include <QMessageBox>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>

AddContactDialog::AddContactDialog(QWidget *parent)
    : QDialog(parent)
    , m_tabWidget(nullptr)
    , m_searchPage(nullptr)
    , m_searchLineEdit(nullptr)
    , m_searchButton(nullptr)
    , m_searchResultsArea(nullptr)
    , m_searchResultsWidget(nullptr)
    , m_searchResultsLayout(nullptr)
    , m_searchStatusLabel(nullptr)
    , m_requestsPage(nullptr)
    , m_refreshRequestsButton(nullptr)
    , m_requestsScrollArea(nullptr)
    , m_requestsListWidget(nullptr)
    , m_requestsListLayout(nullptr)
    , m_requestsStatusLabel(nullptr)
    , m_currentUid(0)
{
    setupUI();
    setupConnections();
    setWindowTitle("添加联系人");
    setMinimumSize(650, 500);
    resize(700, 600);
}

AddContactDialog::~AddContactDialog()
{
}

void AddContactDialog::setupUI()
{
    setWindowTitle("添加联系人");
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // 创建标签页
    m_tabWidget = new QTabWidget(this);
    
    // 搜索用户页面
    setupSearchPage();
    
    // 联系人请求页面
    setupRequestsPage();
    
    // 添加标签页
    m_tabWidget->addTab(m_searchPage, "🔍 搜索用户");
    m_tabWidget->addTab(m_requestsPage, "📩 联系人请求");
    
    mainLayout->addWidget(m_tabWidget);
    
    // 设置现代化样式
    setStyleSheet(R"(
        QDialog {
            background-color: #FFFFFF;
            border-radius: 12px;
        }
        QTabWidget::pane {
            border: 1px solid #E8EAED;
            border-radius: 8px;
            background-color: #FFFFFF;
            margin-top: 8px;
        }
        QTabWidget::tab-bar {
            alignment: center;
        }
        QTabBar::tab {
            background-color: #F8F9FA;
            border: 1px solid #E1E5E9;
            border-bottom: none;
            border-radius: 8px 8px 0px 0px;
            padding: 12px 24px;
            margin-right: 4px;
            font-weight: 500;
            color: #495057;
            font-size: 13px;
        }
        QTabBar::tab:selected {
            background-color: #FFFFFF;
            color: #0078D4;
            border-color: #0078D4;
            font-weight: 600;
        }
        QTabBar::tab:hover:!selected {
            background-color: #F0F2F5;
        }
        QLineEdit {
            border: 1px solid #E1E5E9;
            border-radius: 8px;
            padding: 12px 16px;
            background-color: #FFFFFF;
            font-size: 13px;
            color: #333333;
            selection-background-color: #DCEBFE;
        }
        QLineEdit:focus {
            border: 2px solid #0078D4;
            padding: 11px 15px;
        }
        QPushButton {
            background-color: #0078D4;
            color: white;
            border: none;
            padding: 12px 24px;
            border-radius: 8px;
            font-weight: 600;
            font-size: 13px;
            min-height: 20px;
        }
        QPushButton:hover {
            background-color: #006CBF;
        }
        QPushButton:pressed {
            background-color: #005AA6;
        }
        QPushButton:disabled {
            background-color: #CED4DA;
            color: #6C757D;
        }
        QPushButton#refreshButton {
            background-color: #F8F9FA;
            color: #495057;
            border: 1px solid #CED4DA;
        }
        QPushButton#refreshButton:hover {
            background-color: #E9ECEF;
            border-color: #ADB5BD;
        }
        QLabel {
            color: #333333;
            font-size: 13px;
        }
        QScrollArea {
            border: 1px solid #E8EAED;
            border-radius: 8px;
            background-color: #FAFBFC;
        }
    )");
}

void AddContactDialog::setupSearchPage()
{
    m_searchPage = new QWidget();
    QVBoxLayout *searchLayout = new QVBoxLayout(m_searchPage);
    searchLayout->setContentsMargins(20, 20, 20, 20);
    searchLayout->setSpacing(16);
    
    // 搜索区域
    QLabel *searchLabel = new QLabel("搜索用户");
    searchLabel->setStyleSheet("font-size: 16px; font-weight: 600; color: #1A1A1A; margin-bottom: 8px;");
    
    QHBoxLayout *searchInputLayout = new QHBoxLayout();
    m_searchLineEdit = new QLineEdit();
    m_searchLineEdit->setPlaceholderText("输入用户ID、用户名或邮箱地址...");
    
    m_searchButton = new QPushButton("搜索");
    m_searchButton->setMinimumWidth(80);
    
    searchInputLayout->addWidget(m_searchLineEdit, 1);
    searchInputLayout->addWidget(m_searchButton);
    
    // 状态标签
    m_searchStatusLabel = new QLabel("输入关键词开始搜索用户");
    m_searchStatusLabel->setStyleSheet("color: #6C757D; font-style: italic; margin: 8px 0px;");
    
    // 搜索结果区域
    m_searchResultsArea = new QScrollArea();
    m_searchResultsWidget = new QWidget();
    m_searchResultsLayout = new QVBoxLayout(m_searchResultsWidget);
    m_searchResultsLayout->setAlignment(Qt::AlignTop);
    m_searchResultsLayout->setContentsMargins(0, 0, 0, 0);
    m_searchResultsLayout->setSpacing(8);
    
    m_searchResultsArea->setWidget(m_searchResultsWidget);
    m_searchResultsArea->setWidgetResizable(true);
    m_searchResultsArea->setMinimumHeight(300);
    
    searchLayout->addWidget(searchLabel);
    searchLayout->addLayout(searchInputLayout);
    searchLayout->addWidget(m_searchStatusLabel);
    searchLayout->addWidget(m_searchResultsArea, 1);
}

void AddContactDialog::setupRequestsPage()
{
    m_requestsPage = new QWidget();
    QVBoxLayout *requestsLayout = new QVBoxLayout(m_requestsPage);
    requestsLayout->setContentsMargins(20, 20, 20, 20);
    requestsLayout->setSpacing(16);
    
    // 标题和刷新按钮
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *requestsLabel = new QLabel("联系人请求");
    requestsLabel->setStyleSheet("font-size: 16px; font-weight: 600; color: #1A1A1A;");
    
    m_refreshRequestsButton = new QPushButton("刷新");
    m_refreshRequestsButton->setObjectName("refreshButton");
    m_refreshRequestsButton->setMinimumWidth(80);
    
    headerLayout->addWidget(requestsLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_refreshRequestsButton);
    
    // 状态标签
    m_requestsStatusLabel = new QLabel("正在加载联系人请求...");
    m_requestsStatusLabel->setStyleSheet("color: #6C757D; font-style: italic; margin: 8px 0px;");
    
    // 请求列表区域
    m_requestsScrollArea = new QScrollArea();
    m_requestsListWidget = new QWidget();
    m_requestsListLayout = new QVBoxLayout(m_requestsListWidget);
    m_requestsListLayout->setAlignment(Qt::AlignTop);
    m_requestsListLayout->setContentsMargins(0, 0, 0, 0);
    m_requestsListLayout->setSpacing(8);
    
    m_requestsScrollArea->setWidget(m_requestsListWidget);
    m_requestsScrollArea->setWidgetResizable(true);
    m_requestsScrollArea->setMinimumHeight(300);
    
    requestsLayout->addLayout(headerLayout);
    requestsLayout->addWidget(m_requestsStatusLabel);
    requestsLayout->addWidget(m_requestsScrollArea, 1);
}

void AddContactDialog::setupConnections()
{
    // 搜索功能连接
    connect(m_searchButton, &QPushButton::clicked, this, &AddContactDialog::onSearchButtonClicked);
    connect(m_searchLineEdit, &QLineEdit::textChanged, this, &AddContactDialog::onSearchTextChanged);
    connect(m_searchLineEdit, &QLineEdit::returnPressed, this, &AddContactDialog::onSearchButtonClicked);
    
    // 请求功能连接
    connect(m_refreshRequestsButton, &QPushButton::clicked, this, &AddContactDialog::onRefreshRequestsClicked);
    
    // 连接ContactManager信号
    auto contactMgr = ContactManager::getInstance();
    connect(contactMgr.get(), &ContactManager::userSearchCompleted, 
            this, &AddContactDialog::onUserSearchCompleted);
    connect(contactMgr.get(), &ContactManager::contactRequestsUpdated, 
            this, &AddContactDialog::onContactRequestsUpdated);
    connect(contactMgr.get(), &ContactManager::contactRequestSent, 
            this, &AddContactDialog::onContactRequestSent);
    connect(contactMgr.get(), &ContactManager::contactRequestHandled, 
            this, &AddContactDialog::onContactRequestHandled);
}

void AddContactDialog::setCurrentUser(uint64_t uid, const QString &token)
{
    m_currentUid = uid;
    m_currentToken = token;
}

void AddContactDialog::refreshData()
{
    if (m_currentUid == 0) return;
    
    // 刷新联系人请求
    auto contactMgr = ContactManager::getInstance();
    contactMgr->getContactRequests(m_currentUid, m_currentToken, PENDING);
}

void AddContactDialog::onSearchButtonClicked()
{
    QString searchText = m_searchLineEdit->text().trimmed();
    if (searchText.isEmpty()) {
        m_searchStatusLabel->setText("请输入搜索关键词");
        return;
    }
    
    if (m_currentUid == 0) {
        QMessageBox::warning(this, "错误", "用户信息不完整");
        return;
    }
    
    m_searchButton->setEnabled(false);
    m_searchButton->setText("搜索中...");
    m_searchStatusLabel->setText("正在搜索用户...");
    
    clearSearchResults();
    
    auto contactMgr = ContactManager::getInstance();
    contactMgr->searchUser(m_currentUid, m_currentToken, searchText);
    
    // 5秒后重新启用搜索按钮
    QTimer::singleShot(5000, this, [this]() {
        m_searchButton->setEnabled(true);
        m_searchButton->setText("搜索");
    });
}

void AddContactDialog::onSearchTextChanged()
{
    m_searchButton->setEnabled(!m_searchLineEdit->text().trimmed().isEmpty());
}

void AddContactDialog::onRefreshRequestsClicked()
{
    refreshData();
    m_requestsStatusLabel->setText("正在刷新联系人请求...");
}

void AddContactDialog::onUserSearchCompleted(const QJsonArray &users)
{
    m_searchButton->setEnabled(true);
    m_searchButton->setText("搜索");
    
    clearSearchResults();
    
    if (users.isEmpty()) {
        m_searchStatusLabel->setText("没有找到匹配的用户");
        return;
    }
    
    m_searchStatusLabel->setText(QString("找到 %1 个用户").arg(users.size()));
    
    for (const auto &value : users) {
        QJsonObject user = value.toObject();
        SearchResultItem *item = new SearchResultItem(user);
        connect(item, &SearchResultItem::sendRequestClicked, 
                this, &AddContactDialog::onSendRequestToUser);
        m_searchResultsLayout->addWidget(item);
    }
}

void AddContactDialog::onContactRequestsUpdated()
{
    updateRequestsList();
}

void AddContactDialog::onContactRequestSent(uint64_t requestId)
{
    Q_UNUSED(requestId)
    QMessageBox::information(this, "成功", "联系人请求已发送");
    
    // 切换到请求页面并刷新
    m_tabWidget->setCurrentIndex(1);
    refreshData();
}

void AddContactDialog::onContactRequestHandled(uint64_t requestId, uint64_t action)
{
    Q_UNUSED(requestId)
    if (action == ContactRequestStatus::ACCEPTED) {
        QMessageBox::information(this, "成功", "已接受联系人请求");
        emit contactRequestAccepted(); // 发出信号通知主窗口刷新联系人
    } else {
        QMessageBox::information(this, "完成", "已拒绝联系人请求");
    }
    refreshData();
}

void AddContactDialog::onSendRequestToUser(uint64_t targetUid, const QString &targetName)
{
    QString message = "你好，希望能添加你为联系人。";
    
    auto contactMgr = ContactManager::getInstance();
    contactMgr->sendContactRequest(m_currentUid, m_currentToken, targetUid, message);
}

void AddContactDialog::onAcceptRequest(uint64_t requestId)
{
    auto contactMgr = ContactManager::getInstance();
    contactMgr->handleContactRequest(m_currentUid, m_currentToken, requestId, ContactRequestStatus::ACCEPTED);
}

void AddContactDialog::onRejectRequest(uint64_t requestId)
{
    auto contactMgr = ContactManager::getInstance();
    contactMgr->handleContactRequest(m_currentUid, m_currentToken, requestId, ContactRequestStatus::REJECTED);
}

void AddContactDialog::clearSearchResults()
{
    while (QLayoutItem* item = m_searchResultsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

void AddContactDialog::updateRequestsList()
{
    // 清空现有列表
    while (QLayoutItem* item = m_requestsListLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    
    auto contactMgr = ContactManager::getInstance();
    const auto& requests = contactMgr->getContactRequestsList();
    
    if (requests.isEmpty()) {
        m_requestsStatusLabel->setText("暂无待处理的联系人请求");
        return;
    }
    
    m_requestsStatusLabel->setText(QString("有 %1 个待处理的联系人请求").arg(requests.size()));
    
    for (const auto& request : requests) {
        if (request.status == PENDING) {
            ContactRequestItem *item = new ContactRequestItem(request);
            connect(item, &ContactRequestItem::acceptClicked, 
                    this, &AddContactDialog::onAcceptRequest);
            connect(item, &ContactRequestItem::rejectClicked, 
                    this, &AddContactDialog::onRejectRequest);
            m_requestsListLayout->addWidget(item);
        }
    }
}

// SearchResultItem 实现
SearchResultItem::SearchResultItem(const QJsonObject &userData, QWidget *parent)
    : QFrame(parent)
{
    m_userId = userData["uid"].toVariant().toULongLong();
    m_userName = userData["username"].toString();
    m_userEmail = userData["email"].toString();
    
    setupUI();
}

void SearchResultItem::setupUI()
{
    setFrameStyle(QFrame::Box);
    setStyleSheet(R"(
        QFrame {
            background-color: #FFFFFF;
            border: 1px solid #E8EAED;
            border-radius: 8px;
            padding: 8px;
        }
        QFrame:hover {
            border-color: #DCEBFE;
            background-color: #FAFEFF;
        }
    )");
    
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    
    // 头像
    m_avatarLabel = new QLabel();
    m_avatarLabel->setFixedSize(40, 40);
    QPixmap defaultAvatar(":/resource/image/default-avatar.svg");
    defaultAvatar = defaultAvatar.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_avatarLabel->setPixmap(defaultAvatar);
    m_avatarLabel->setStyleSheet("border-radius: 20px; border: 1px solid #E1E5E9;");
    
    // 用户信息
    QVBoxLayout *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(4);
    
    m_nameLabel = new QLabel(m_userName);
    m_nameLabel->setStyleSheet("font-weight: 600; font-size: 14px; color: #1A1A1A;");
    
    m_emailLabel = new QLabel(m_userEmail);
    m_emailLabel->setStyleSheet("font-size: 12px; color: #6C757D;");
    
    infoLayout->addWidget(m_nameLabel);
    infoLayout->addWidget(m_emailLabel);
    
    // 发送请求按钮
    m_sendRequestButton = new QPushButton("发送请求");
    m_sendRequestButton->setMinimumWidth(100);
    connect(m_sendRequestButton, &QPushButton::clicked, this, &SearchResultItem::onSendRequestClicked);
    
    layout->addWidget(m_avatarLabel);
    layout->addLayout(infoLayout, 1);
    layout->addWidget(m_sendRequestButton);
}

void SearchResultItem::onSendRequestClicked()
{
    emit sendRequestClicked(m_userId, m_userName);
    m_sendRequestButton->setEnabled(false);
    m_sendRequestButton->setText("已发送");
}

// ContactRequestItem 实现
ContactRequestItem::ContactRequestItem(const ContactRequest &request, QWidget *parent)
    : QFrame(parent)
    , m_requestId(request.request_id)
    , m_requesterId(request.requester_id)
    , m_requesterName(request.requester_name)
    , m_requestMessage(request.request_message)
    , m_status(request.status)
{
    setupUI();
}

void ContactRequestItem::setupUI()
{
    setFrameStyle(QFrame::Box);
    setStyleSheet(R"(
        QFrame {
            background-color: #FFFFFF;
            border: 1px solid #E8EAED;
            border-radius: 8px;
            padding: 8px;
        }
    )");
    
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    
    // 头像
    m_avatarLabel = new QLabel();
    m_avatarLabel->setFixedSize(40, 40);
    QPixmap defaultAvatar(":/resource/image/default-avatar.svg");
    defaultAvatar = defaultAvatar.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_avatarLabel->setPixmap(defaultAvatar);
    m_avatarLabel->setStyleSheet("border-radius: 20px; border: 1px solid #E1E5E9;");
    
    // 请求信息
    QVBoxLayout *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(4);
    
    m_nameLabel = new QLabel(m_requesterName);
    m_nameLabel->setStyleSheet("font-weight: 600; font-size: 14px; color: #1A1A1A;");
    
    m_messageLabel = new QLabel(m_requestMessage);
    m_messageLabel->setStyleSheet("font-size: 12px; color: #6C757D;");
    m_messageLabel->setWordWrap(true);
    
    infoLayout->addWidget(m_nameLabel);
    infoLayout->addWidget(m_messageLabel);
    
    // 按钮区域
    QVBoxLayout *buttonLayout = new QVBoxLayout();
    buttonLayout->setSpacing(8);
    
    m_acceptButton = new QPushButton("接受");
    m_acceptButton->setStyleSheet("background-color: #28A745; color: white;");
    m_acceptButton->setMinimumWidth(80);
    
    m_rejectButton = new QPushButton("拒绝");
    m_rejectButton->setStyleSheet("background-color: #DC3545; color: white;");
    m_rejectButton->setMinimumWidth(80);
    
    connect(m_acceptButton, &QPushButton::clicked, this, &ContactRequestItem::onAcceptClicked);
    connect(m_rejectButton, &QPushButton::clicked, this, &ContactRequestItem::onRejectClicked);
    
    buttonLayout->addWidget(m_acceptButton);
    buttonLayout->addWidget(m_rejectButton);
    
    layout->addWidget(m_avatarLabel);
    layout->addLayout(infoLayout, 1);
    layout->addLayout(buttonLayout);
}

void ContactRequestItem::onAcceptClicked()
{
    emit acceptClicked(m_requestId);
    m_acceptButton->setEnabled(false);
    m_rejectButton->setEnabled(false);
    m_acceptButton->setText("已接受");
}

void ContactRequestItem::onRejectClicked()
{
    emit rejectClicked(m_requestId);
    m_acceptButton->setEnabled(false);
    m_rejectButton->setEnabled(false);
    m_rejectButton->setText("已拒绝");
} 