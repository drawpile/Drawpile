// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/widgetutils.h"
#include <QWidget>

namespace utils {

ScopedUpdateDisabler::ScopedUpdateDisabler(QWidget *widget)
	: m_widget{widget}
	, m_wasEnabled{widget->updatesEnabled()}
{
	if(m_wasEnabled) {
		widget->setUpdatesEnabled(false);
	}
}

ScopedUpdateDisabler::~ScopedUpdateDisabler()
{
	if(m_wasEnabled) {
		m_widget->setUpdatesEnabled(true);
	}
}

void showWindow(QWidget *widget, bool maximized)
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

void setWidgetRetainSizeWhenHidden(QWidget *widget, bool retainSize)
{
	QSizePolicy sp = widget->sizePolicy();
	sp.setRetainSizeWhenHidden(retainSize);
	widget->setSizePolicy(sp);
}

}
