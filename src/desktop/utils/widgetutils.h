#ifndef WIDGETUTILS_H
#define WIDGETUTILS_H

#include <QWidget>

namespace utils {

inline void showWindow(QWidget *widget)
{
    // On Android, we rarely want small windows unless it's like a simple
    // message box or something. Anything more complex is probably better off
    // being a full-screen window, which is also more akin to how Android's
    // native UI works. This wrapper takes care of that very common switch.
#ifdef Q_OS_ANDROID
    widget->showFullScreen();
#else
    widget->show();
#endif
}

}

#endif
