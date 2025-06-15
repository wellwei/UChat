#ifndef CONVERSATIONITEMWIDGET_H
#define CONVERSATIONITEMWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QStyleOption>
#include <QDateTime>
#include <QGraphicsDropShadowEffect>

class ConversationItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConversationItemWidget(QWidget *parent = nullptr);
    
    void setConversationInfo(uint64_t uid, const QString &name, 
                           const QString &lastMessage, qint64 lastTime,
                           const QString &avatarPath = QString(),
                           int unreadCount = 0);
    
    uint64_t getUid() const { return m_uid; }
    
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    
protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

signals:
    void clicked(uint64_t uid);

private:
    void setupUI();
    void updateTimeLabel();
    void updateUnreadBadge();
    QString formatTime(qint64 timestamp);
    QPixmap createRoundedPixmap(const QPixmap &source, int radius);

private:
    uint64_t m_uid;
    QString m_name;
    QString m_lastMessage;
    qint64 m_lastTime;
    QString m_avatarPath;
    int m_unreadCount;
    bool m_selected;
    bool m_hovered;
    
    // UI组件
    QLabel *m_avatarLabel;
    QLabel *m_nameLabel;
    QLabel *m_messageLabel;
    QLabel *m_timeLabel;
    QLabel *m_unreadBadge;
    
    QHBoxLayout *m_mainLayout;
    QVBoxLayout *m_contentLayout;
    QVBoxLayout *m_rightLayout;
};

#endif // CONVERSATIONITEMWIDGET_H 