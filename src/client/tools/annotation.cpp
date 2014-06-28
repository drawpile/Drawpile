/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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

#include <QApplication>

#include "scene/canvasscene.h"
#include "docks/toolsettingsdock.h"
#include "net/client.h"
#include "core/annotation.h"

#include "tools/toolsettings.h"
#include "tools/annotation.h"
#include "tools/utils.h"

namespace tools {

/**
 * The annotation tool has fairly complex needs. Clicking on an existing
 * annotation selects it, otherwise a new annotation is started.
 */
void Annotation::begin(const paintcore::Point& point, bool right)
{
	Q_UNUSED(right);

	drawingboard::AnnotationItem *item = scene().annotationAt(point.toPoint());
	if(item) {
		_selected = item;
		_handle = _selected->handleAt(point.toPoint());
		settings().getAnnotationSettings()->setSelection(item);
		_wasselected = true;
	} else {
		QGraphicsRectItem *item = new QGraphicsRectItem();
		QPen pen;
		pen.setWidth(1);
		pen.setCosmetic(true);
		pen.setColor(QApplication::palette().color(QPalette::Highlight));
		pen.setStyle(Qt::DotLine);
		item->setPen(pen);
		item->setRect(QRectF(point, point));
		scene().setToolPreview(item);
		_p2 = point;
		_wasselected = false;
	}
	_start = point;
	_p1 = point;
}

/**
 * If we have a selected annotation, move or resize it. Otherwise extend
 * the preview rectangle for the new annotation.
 */
void Annotation::motion(const paintcore::Point& point, bool constrain, bool center)
{
	if(_wasselected) {
		// Annotation may have been deleted by other user while we were moving it.
		if(_selected) {
			QPointF p = point - _start;
			// TODO constrain
			_selected->adjustGeometry(_handle, p.toPoint());
			_start = point;
		}
	} else {
		if(constrain)
			_p2 = constraints::square(_start, point);
		else
			_p2 = point;

		if(center)
			_p1 = _start - (_p2 - _start);
		else
			_p1 = _start;

		QGraphicsRectItem *item = qgraphicsitem_cast<QGraphicsRectItem*>(scene().toolPreview());
		if(item)
			item->setRect(QRectF(_p1, _p2).normalized());
	}
}

/**
 * If we have a selected annotation, adjust its shape.
 * Otherwise, create a new annotation.
 */
void Annotation::end()
{
	if(_wasselected) {
		if(_selected) {
			client().sendAnnotationReshape(_selected->id(), _selected->geometry().toRect());
			_selected = 0;
		}
	} else {
		scene().setToolPreview(0);

		QRect rect = QRect(_p1.toPoint(), _p2.toPoint()).normalized();

		if(rect.width() < 10 && rect.height() < 10) {
			// User created a tiny annotation, probably by clicking rather than dragging.
			// Create a nice and big annotation box rather than a minimum size one.
			rect.setSize(QSize(160, 60));
		}

		client().sendAnnotationCreate(0, rect);
	}
}

}

