#ifndef MESSAGEDELEGATE_H
#define MESSAGEDELEGATE_H

#include <QStyledItemDelegate>

class MessageDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit MessageDelegate(QObject *parent = nullptr);

    // QAbstractItemDelegate interface
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    // 辅助函数，用于计算文本绘制区域
    QRect getMessageRect(const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

#endif // MESSAGEDELEGATE_H 