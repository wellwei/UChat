//
// Created by wellwei on 2025/4/11.
//

#include "timerbutton.hpp"
#include <QTimer>

TimerButton::TimerButton(QWidget *parent)
        : QPushButton(parent),
          m_timer(new QTimer(this)),
          m_totalSeconds(0),
          m_remainingSeconds(0) {
    connect(m_timer, &QTimer::timeout, this, &TimerButton::onTimeout);
}

TimerButton::~TimerButton() {
    if (m_timer->isActive()) {
        m_timer->stop();
    }
}

void TimerButton::startCountdown(int seconds, const QString &countdownFormat) {
    if (m_timer->isActive() || seconds <= 0) {
        return; // 如果计时器已经在运行或者秒数无效，则不启动
    }
    m_totalSeconds = seconds;
    m_remainingSeconds = seconds;
    m_countdownFormat = countdownFormat;
    m_originalText = text(); // 保存原始文本

    setEnabled(false);
    updateButtonText();
    m_timer->start(1000); // 每秒触发一次
}

void TimerButton::onTimeout() {
    --m_remainingSeconds;
    if (m_remainingSeconds <= 0) {
        m_timer->stop();
        setEnabled(true);
        setText(m_originalText); // 恢复原始文本
        m_remainingSeconds = 0;
    } else {
        updateButtonText();
    }
}

void TimerButton::updateButtonText() {
    setText(m_countdownFormat.arg(m_remainingSeconds));
}