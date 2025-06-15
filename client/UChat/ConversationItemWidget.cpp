#include "ConversationItemWidget.h"

#include <QFile>
#include <QMouseEvent>
#include <QPainterPath>
#include <QFontMetrics>

ConversationItemWidget::ConversationItemWidget(QWidget *parent)
    : QWidget(parent)
    , m_uid(0)
    , m_lastTime(0)
    , m_unreadCount(0)
    , m_selected(false)
    , m_hovered(false)
    , m_avatarLabel(nullptr)
    , m_nameLabel(nullptr)
    , m_messageLabel(nullptr)
    , m_timeLabel(nullptr)
    , m_unreadBadge(nullptr)
    , m_mainLayout(nullptr)
    , m_contentLayout(nullptr)
    , m_rightLayout(nullptr)
{
    setupUI();
    setFixedHeight(72);
    setMouseTracking(true);
}

void ConversationItemWidget::setupUI()
{
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(12, 8, 12, 8);
    m_mainLayout->setSpacing(12);
    
    // 头像
    m_avatarLabel = new QLabel();
    m_avatarLabel->setFixedSize(48, 48);
    m_avatarLabel->setScaledContents(false); // 改为false确保图片完整显示
    m_avatarLabel->setAlignment(Qt::AlignCenter); // 居中对齐
    m_avatarLabel->setStyleSheet(
        "QLabel {"
        "    border-radius: 24px;"
        "    background-color: #E8F4FD;"
        "    border: 1px solid #B8E6FF;" // 减小边框避免遮挡
        "    padding: 0px;" // 确保无内边距
        "}"
    );
    
    // 内容区域
    m_contentLayout = new QVBoxLayout();
    m_contentLayout->setSpacing(2);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    
    // 姓名标签
    m_nameLabel = new QLabel();
    m_nameLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 14px;"
        "    font-weight: 600;"
        "    color: #1A1A1A;"
        "}"
    );
    
    // 消息标签
    m_messageLabel = new QLabel();
    m_messageLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 12px;"
        "    color: #6C757D;"
        "    font-weight: 400;"
        "}"
    );
    m_messageLabel->setWordWrap(false);
    
    m_contentLayout->addWidget(m_nameLabel);
    m_contentLayout->addWidget(m_messageLabel);
    m_contentLayout->addStretch();
    
    // 右侧区域
    m_rightLayout = new QVBoxLayout();
    m_rightLayout->setSpacing(4);
    m_rightLayout->setContentsMargins(0, 0, 0, 0);
    m_rightLayout->setAlignment(Qt::AlignTop | Qt::AlignRight);
    
    // 时间标签
    m_timeLabel = new QLabel();
    m_timeLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 11px;"
        "    color: #8E9297;"
        "    font-weight: 400;"
        "}"
    );
    m_timeLabel->setAlignment(Qt::AlignRight);
    
    // 未读消息徽章
    m_unreadBadge = new QLabel();
    m_unreadBadge->setFixedSize(18, 18);
    m_unreadBadge->setAlignment(Qt::AlignCenter);
    m_unreadBadge->setStyleSheet(
        "QLabel {"
        "    background-color: #FF4757;"
        "    color: white;"
        "    border-radius: 9px;"
        "    font-size: 10px;"
        "    font-weight: 600;"
        "}"
    );
    m_unreadBadge->hide();
    
    m_rightLayout->addWidget(m_timeLabel);
    m_rightLayout->addWidget(m_unreadBadge);
    m_rightLayout->addStretch();
    
    // 组装布局
    m_mainLayout->addWidget(m_avatarLabel);
    m_mainLayout->addLayout(m_contentLayout, 1);
    m_mainLayout->addLayout(m_rightLayout);
}

void ConversationItemWidget::setConversationInfo(uint64_t uid, const QString &name,
                                                const QString &lastMessage, qint64 lastTime,
                                                const QString &avatarPath, int unreadCount)
{
    m_uid = uid;
    m_name = name;
    m_lastMessage = lastMessage;
    m_lastTime = lastTime;
    m_avatarPath = avatarPath;
    m_unreadCount = unreadCount;
    
    // 更新UI
    m_nameLabel->setText(name);
    
    // 截断过长的消息
    QFontMetrics fm(m_messageLabel->font());
    QString elidedMessage = fm.elidedText(lastMessage, Qt::ElideRight, 200);
    m_messageLabel->setText(elidedMessage);
    
    updateTimeLabel();
    updateUnreadBadge();
    
    // 统一使用default-avatar.svg
    QPixmap defaultAvatar(":/resource/image/default-avatar.svg");
    if (!defaultAvatar.isNull()) {
        // 确保头像适配到正确尺寸并创建圆形
        defaultAvatar = defaultAvatar.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmap roundedAvatar = createRoundedPixmap(defaultAvatar, 24);
        m_avatarLabel->setPixmap(roundedAvatar);
    } else {
        // 如果SVG加载失败，使用纯色圆形作为后备
        QPixmap fallbackAvatar(48, 48);
        fallbackAvatar.fill(Qt::transparent);
        
        QPainter painter(&fallbackAvatar);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // 绘制背景圆
        painter.setBrush(QColor("#E8F4FD"));
        painter.setPen(QPen(QColor("#B8E6FF"), 2));
        painter.drawEllipse(2, 2, 44, 44);
        
        // 绘制首字母
        if (!name.isEmpty()) {
            painter.setPen(QColor("#0078D4"));
            painter.setFont(QFont("Microsoft YaHei", 16, QFont::Bold));
            painter.drawText(fallbackAvatar.rect(), Qt::AlignCenter, name.left(1).toUpper());
        }
        
        m_avatarLabel->setPixmap(fallbackAvatar);
    }
}

void ConversationItemWidget::setSelected(bool selected)
{
    if (m_selected != selected) {
        m_selected = selected;
        update();
    }
}

void ConversationItemWidget::updateTimeLabel()
{
    m_timeLabel->setText(formatTime(m_lastTime));
}

void ConversationItemWidget::updateUnreadBadge()
{
    if (m_unreadCount > 0) {
        QString text = m_unreadCount > 99 ? "99+" : QString::number(m_unreadCount);
        m_unreadBadge->setText(text);
        m_unreadBadge->show();
        
        // 根据数字长度调整大小
        int width = m_unreadCount > 99 ? 24 : 18;
        m_unreadBadge->setFixedSize(width, 18);
    } else {
        m_unreadBadge->hide();
    }
}

QString ConversationItemWidget::formatTime(qint64 timestamp)
{
    if (timestamp == 0) return QString();
    
    QDateTime msgTime = QDateTime::fromSecsSinceEpoch(timestamp);
    QDateTime now = QDateTime::currentDateTime();
    
    qint64 secondsAgo = msgTime.secsTo(now);
    
    if (secondsAgo < 60) {
        return "刚刚";
    } else if (secondsAgo < 3600) {
        return QString("%1分钟前").arg(secondsAgo / 60);
    } else if (msgTime.date() == now.date()) {
        return msgTime.toString("hh:mm");
    } else if (msgTime.date() == now.date().addDays(-1)) {
        return "昨天";
    } else if (secondsAgo < 7 * 24 * 3600) {
        return QString("%1天前").arg(secondsAgo / (24 * 3600));
    } else {
        return msgTime.toString("MM/dd");
    }
}

QPixmap ConversationItemWidget::createRoundedPixmap(const QPixmap &source, int radius)
{
    if (source.isNull()) return QPixmap();
    
    QPixmap rounded(source.size());
    rounded.fill(Qt::transparent);
    
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QPainterPath path;
    path.addRoundedRect(rounded.rect(), radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, source);
    
    return rounded;
}

void ConversationItemWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect rect = this->rect();
    
    if (m_selected) {
        // 选中状态
        QLinearGradient gradient(0, 0, rect.width(), 0);
        gradient.setColorAt(0, QColor("#E3F2FD"));
        gradient.setColorAt(1, QColor("#F0F8FF"));
        painter.fillRect(rect, gradient);
        
        // 左侧选中指示器
        painter.fillRect(0, 0, 4, rect.height(), QColor("#0078D4"));
        
        // 边框
        painter.setPen(QPen(QColor("#DCEBFE"), 1));
        painter.drawRoundedRect(rect.marginsRemoved(QMargins(2, 2, 2, 2)), 8, 8);
    } else if (m_hovered) {
        // 悬停状态
        QLinearGradient gradient(0, 0, rect.width(), 0);
        gradient.setColorAt(0, QColor("#F8FAFE"));
        gradient.setColorAt(1, QColor("#F2F7FE"));
        painter.fillRect(rect.marginsRemoved(QMargins(2, 2, 2, 2)), gradient);
        painter.setPen(QPen(QColor("#E8F0FF"), 1));
        painter.drawRoundedRect(rect.marginsRemoved(QMargins(2, 2, 2, 2)), 6, 6);
    }
    
    // 底部分隔线
    if (!m_selected) {
        painter.setPen(QPen(QColor("#F0F2F5"), 1));
        painter.drawLine(60, rect.height() - 1, rect.width() - 12, rect.height() - 1);
    }
}

void ConversationItemWidget::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event)
    m_hovered = true;
    update();
}

void ConversationItemWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    m_hovered = false;
    update();
}

void ConversationItemWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_uid);
    }
    QWidget::mousePressEvent(event);
} 