// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WIDGETUTILS_H
#define WIDGETUTILS_H

class QWidget;

namespace utils {

class ScopedUpdateDisabler {
public:
	ScopedUpdateDisabler(QWidget *widget);
	~ScopedUpdateDisabler();

private:
	QWidget *m_widget;
	bool m_wasEnabled;
};

void showWindow(QWidget *widget, bool maximized = false);

void setWidgetRetainSizeWhenHidden(QWidget *widget, bool retainSize);

}

#endif
