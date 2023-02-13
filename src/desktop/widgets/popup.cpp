/*
 * Drawpile - a collaborative drawing program.
 *
 * Copyright (C) 2023 askmeaboutloom
 *
 * Drawpile is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Drawpile is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "popup.h"
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
#	include <QScreen>
#else
#	include <QApplication>
#	include <QDesktopWidget>
#endif

namespace widgets {

Popup::Popup(QWidget *parent)
	: QFrame{parent}
{
	setWindowFlags(Qt::FramelessWindowHint | Qt::Popup);
	setFrameStyle(QFrame::StyledPanel);
	setFrameShadow(QFrame::Raised);
}

void Popup::setVisibleOn(QWidget *widget, bool visible)
{
	if(visible) {
		showOn(widget);
	} else {
		hide();
	}
}

void Popup::showOn(QWidget *widget)
{
	Q_ASSERT(widget);
	emit targetChanged(widget);

	// Move the widget into a place where it doesn't spill off-screen.
	QPoint pos = findPosition(widget);
	move(pos);
	show();
}

void Popup::hideEvent(QHideEvent *)
{
	targetChanged(nullptr);
}

QPoint Popup::findPosition(const QWidget *widget) const
{
	QRect widgetArea = widget->rect();
	QPoint below = widget->mapToGlobal(widgetArea.bottomLeft()) + QPoint{0, 1};
	QRect screenArea;
	if(getScreenArea(widget, screenArea)) {
		QRect widgetArea = widget->rect();
		QSize popupSize = sizeHint();
		// Try to place the popup below the button.
		if(tryPosition(screenArea, popupSize, below)) {
			return below;
		}
		// That didn't fit, try to place it above.
		QPoint above = widget->mapToGlobal(widgetArea.topLeft()) -
					   QPoint{0, popupSize.height()};
		if(tryPosition(screenArea, popupSize, above)) {
			return above;
		}
		// Okay apparently the popup doesn't fit on the screen at all or
		// something. Just punt to placing it below anyway then.
	}
	return below;
}

bool Popup::getScreenArea(const QWidget *widget, QRect &outRect)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	QScreen *screen = widget->screen();
	if(screen) {
		outRect = screen->availableGeometry();
		return true;
	} else {
		return false;
	}
#else
	QDesktopWidget *desktop = QApplication::desktop();
	if(desktop) {
		outRect = desktop->availableGeometry(widget);
		return true;
	} else {
		return false;
	}
#endif
}

bool Popup::tryPosition(
	const QRect &screenArea, const QSize &popupSize, QPoint &inOutPoint)
{
	QRect area = QRect{inOutPoint, popupSize};
	if(area.top() >= screenArea.top() && area.bottom() <= screenArea.bottom()) {
		if(area.right() > screenArea.right()) {
			inOutPoint -= QPoint{area.right() - screenArea.right(), 0};
		} else if(area.left() < screenArea.left()) {
			inOutPoint += QPoint{screenArea.left() - area.left(), 0};
		}
		return true;
	} else {
		return false;
	}
}

}
