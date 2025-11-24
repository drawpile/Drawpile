// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_UTILS_QTGUICOMPAT_H
#define DESKTOP_UTILS_QTGUICOMPAT_H
#include "libclient/utils/qtguicompat.h"

class QScreen;
class QStyle;
class QWidget;

namespace compat {

QScreen *widgetScreen(const QWidget &widget);
QScreen *widgetOrPrimaryScreen(const QWidget &widget);

QString styleName(const QStyle &style);

// Do not attempt to replace these #defines with something more C++ish.
// That gets miscompiled on MSVC and your signals will not connect.
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
using CheckBoxState = Qt::CheckState;
#	define COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(CLS) &CLS::checkStateChanged
#else
using CheckBoxState = int;
#	define COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(CLS) &CLS::stateChanged
#endif

}

#endif
