/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "desktop/utils/hotbordereventfilter.h"

#include <QMouseEvent>
#include <QWidget>

HotBorderEventFilter::HotBorderEventFilter(QObject *parent)
	: QObject(parent), m_hotBorder(false)
{
}

bool HotBorderEventFilter::eventFilter(QObject *object, QEvent *event)
{
	if(event->type() == QEvent::HoverMove) {
		const QMouseEvent *e = static_cast<const QMouseEvent*>(event);

		// For some reason e->globalPos() does not always work. Window manager specific problem?
		const QPoint p = static_cast<QWidget*>(object)->mapToGlobal(e->pos());

		if(m_hotBorder) {
			if(p.y() > 30) {
				m_hotBorder = false;
				emit hotBorder(false);
			}
		} else {
			if(p.y() < 10) {
				m_hotBorder = true;
				emit hotBorder(true);
			}
		}
	}

	return false;
}

