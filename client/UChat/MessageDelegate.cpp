#include "MessageDelegate.h"
#include "ChatMessageModel.h"
#include <QDateTime>
#include <QPainter>
#include <QPainterPath>
#include <QTextDocument>

namespace {
    // 定义常量以方便维护
    const int AVATAR_SIZE = 40;
    const int HORIZONTAL_MARGIN = 10;
    const int VERTICAL_MARGIN = 5;
    const int BUBBLE_PADDING = 10;
    const int BUBBLE_RADIUS = 8;
    const int TIME_HEIGHT = 15;
}

MessageDelegate::MessageDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}

void MessageDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // --- 1. Get Data ---
    bool isOutgoing = index.data(ChatMessage::IsOutgoingRole).toBool();
    QString content = index.data(ChatMessage::ContentRole).toString();
    qint64 timestamp = index.data(ChatMessage::TimestampRole).toLongLong();
    QString avatarPath = index.data(ChatMessage::AvatarPathRole).toString();

    // --- 2. Calculate Geometry ---
    QTextDocument doc;
    doc.setHtml(content);
    int bubbleMaxWidth = option.rect.width() - 2 * (HORIZONTAL_MARGIN + AVATAR_SIZE + HORIZONTAL_MARGIN);
    doc.setTextWidth(bubbleMaxWidth - 2 * BUBBLE_PADDING);

    int textHeight = doc.size().height();
    int bubbleHeight = textHeight + 2 * BUBBLE_PADDING + TIME_HEIGHT;
    int bubbleWidth = doc.size().width() + 2 * BUBBLE_PADDING;

    QRect avatarRect;
    QRect bubbleRect;

    if (isOutgoing) {
        // Layout for outgoing messages (Right-aligned)
        avatarRect = QRect(option.rect.right() - HORIZONTAL_MARGIN - AVATAR_SIZE,
                           option.rect.top() + VERTICAL_MARGIN,
                           AVATAR_SIZE, AVATAR_SIZE);

        bubbleRect = QRect(avatarRect.left() - HORIZONTAL_MARGIN - bubbleWidth,
                           option.rect.top() + VERTICAL_MARGIN,
                           bubbleWidth, bubbleHeight);
    } else {
        // Layout for incoming messages (Left-aligned)
        avatarRect = QRect(option.rect.left() + HORIZONTAL_MARGIN,
                           option.rect.top() + VERTICAL_MARGIN,
                           AVATAR_SIZE, AVATAR_SIZE);

        bubbleRect = QRect(avatarRect.right() + HORIZONTAL_MARGIN,
                           option.rect.top() + VERTICAL_MARGIN,
                           bubbleWidth, bubbleHeight);
    }

    // --- 3. Painting ---

    // Draw Avatar
    QPixmap avatarPixmap(avatarPath.isEmpty() ? ":/resource/image/default-avatar.svg" : avatarPath);
    QPainterPath path;
    path.addEllipse(avatarRect);
    painter->setClipPath(path);
    painter->drawPixmap(avatarRect, avatarPixmap);
    painter->setClipping(false);

    // Draw Bubble
    QColor bubbleColor = isOutgoing ? QColor("#1296db") : QColor("#FFFFFF");
    painter->setBrush(bubbleColor);
    if (isOutgoing) {
        painter->setPen(Qt::NoPen); // 发送方消息无边框
    } else {
        painter->setPen(QColor("#E0E0E0")); // 接收方消息有浅灰色边框
    }
    painter->drawRoundedRect(bubbleRect, BUBBLE_RADIUS, BUBBLE_RADIUS);

    // Draw Content Text
    QRect textRect = bubbleRect.adjusted(BUBBLE_PADDING, BUBBLE_PADDING, -BUBBLE_PADDING, -BUBBLE_PADDING - TIME_HEIGHT);
    painter->setPen(isOutgoing ? Qt::white : Qt::black);
    painter->translate(textRect.topLeft());
    doc.drawContents(painter);
    painter->translate(-textRect.topLeft());

    // Draw Timestamp
    QRect timeRect(textRect.left(), textRect.bottom(), textRect.width(), TIME_HEIGHT);
    painter->setPen(Qt::darkGray); // 时间戳使用深灰色，更清晰
    painter->setFont(QFont("Arial", 8));
    QString timeString = QDateTime::fromSecsSinceEpoch(timestamp).toString("HH:mm");
    Qt::AlignmentFlag align = isOutgoing ? Qt::AlignRight : Qt::AlignLeft;
    painter->drawText(timeRect, align | Qt::AlignVCenter, timeString);

    painter->restore();
}

QSize MessageDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QString content = index.data(ChatMessage::ContentRole).toString();
    
    int bubbleMaxWidth = option.rect.width() - 2 * (HORIZONTAL_MARGIN + AVATAR_SIZE);
    
    QTextDocument doc;
    doc.setHtml(content);
    doc.setTextWidth(bubbleMaxWidth - 2 * BUBBLE_PADDING);
    
    int textHeight = doc.size().height();
    int bubbleHeight = textHeight + 2 * BUBBLE_PADDING + TIME_HEIGHT;
    
    int finalHeight = qMax(AVATAR_SIZE, bubbleHeight) + 2 * VERTICAL_MARGIN;
    
    return QSize(option.rect.width(), finalHeight);
} 