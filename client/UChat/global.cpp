//
// Created by wellwei on 2025/3/28.
//

#include "global.h"
#include <QStyle>

QString GATE_SERVER_URL = "";

// 刷新 w 控件的 style
std::function<void(QWidget *)> repolish = [](QWidget *w) {
    if (w) {
        w->style()->unpolish(w);
        w->style()->polish(w);
        w->update();
    }
};
