// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WIDGETUTILS_H
#define WIDGETUTILS_H

#include <QWidget>

namespace utils {

inline void showWindow(QWidget *widget, bool maximized = false)
{
    // On Android, we rarely want small windows unless it's like a simple
    // message box or something. Anything more complex is probably better off
    // being a full-screen window, which is also more akin to how Android's
    // native UI works. This wrapper takes care of that very common switch.
#ifdef Q_OS_ANDROID
    Q_UNUSED(maximized);
    widget->showFullScreen();
#else
    if(maximized) {
        widget->showMaximized();
    } else {
        widget->show();
    }
#endif
}

inline void setWidgetRetainSizeWhenHidden(QWidget *widget, bool retainSize)
{
	QSizePolicy sp = widget->sizePolicy();
	sp.setRetainSizeWhenHidden(retainSize);
	widget->setSizePolicy(sp);
}

}

#endif
