//
// Created by wellwei on 2025/4/11.
//

#ifndef UCHAT_TIMERBUTTON_HPP
#define UCHAT_TIMERBUTTON_HPP

#include <QPushButton>

class TimerButton : public QPushButton {
Q_OBJECT
public:
    explicit TimerButton(QWidget *parent = nullptr);

    ~TimerButton() override;

    /**
     * @brief 启动倒计时
     * @param seconds 倒计时的总秒数 (例如 60)
     * @param countdownFormat 倒计时期间显示的文本格式，%1 会被替换为剩余秒数。默认为 "重试 (%1s)"
     */
    void startCountdown(int seconds, const QString &countdownFormat = tr("重试 (%1s)"));

private slots:

    void onTimeout(); // 处理计时器超时的槽函数
private:
    void updateButtonText(); // 辅助函数更新按钮文本
    QTimer *m_timer;           // 计时器对象
    int m_totalSeconds;        // 倒计时的总秒数
    int m_remainingSeconds;    // 剩余的秒数
    QString m_originalText;    // 按钮的原始文本
    QString m_countdownFormat; // 倒计时期间的文本格式
};

#endif //UCHAT_TIMERBUTTON_HPP
