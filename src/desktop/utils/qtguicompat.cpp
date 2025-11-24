// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/qtguicompat.h"
#include <QApplication>
#include <QStyle>
#include <QWidget>

namespace compat {

QScreen *widgetScreen(const QWidget &widget)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	return widget.screen();
#else
	Q_UNUSED(widget);
	return qApp->primaryScreen();
#endif
}

QScreen *widgetOrPrimaryScreen(const QWidget &widget)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	QScreen *screen = widget.screen();
	if(screen) {
		return screen;
	} else {
		return qApp->primaryScreen();
	}
#else
	Q_UNUSED(widget);
	return qApp->primaryScreen();
#endif
}

QString styleName(const QStyle &style)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return style.name();
#else
	return style.objectName();
#endif
}

}
