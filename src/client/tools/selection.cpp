/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2015 Calle Laakkonen

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

#include "scene/selectionitem.h"
#include "scene/canvasscene.h"
#include "net/client.h"

#include "tools/selection.h"
#include "tools/utils.h"

namespace tools {

RectangleSelection::RectangleSelection(ToolCollection &owner)
	: Tool(owner, SELECTION, QCursor(QPixmap(":cursors/select-rectangle.png"), 2, 2))
{
}

void RectangleSelection::begin(const paintcore::Point &point, bool right, float zoom)
{
	// Right click to dismiss selection (and paste buffer)
	if(right) {
		scene().setSelectionItem(nullptr);
		return;
	}

	if(scene().selectionItem())
		_handle = scene().selectionItem()->handleAt(point.toPoint(), zoom);
	else
		_handle = drawingboard::SelectionItem::OUTSIDE;

	_start = point.toPoint();
	_p1 = _start;

	if(_handle == drawingboard::SelectionItem::OUTSIDE) {
		if(scene().selectionItem())
			scene().selectionItem()->pasteToCanvas(&client(), layer());

		scene().setSelectionItem(QRectF(point, point).toRect());
	}
}

void RectangleSelection::motion(const paintcore::Point &point, bool constrain, bool center)
{
	if(!scene().selectionItem())
		return;

	if(_handle==drawingboard::SelectionItem::OUTSIDE) {
		QPointF p;
		if(constrain)
			p = constraints::square(_start, point);
		else
			p = point;

		if(center)
			_p1 = _start - (p.toPoint() - _start);
		else
			_p1 = _start;

		scene().selectionItem()->setRect(QRectF(_p1, p).normalized().toRect());
	} else {
		// TODO constrain
		QPointF p = point - _start;
		scene().selectionItem()->adjustGeometry(_handle, p.toPoint());
		_start = point.toPoint();
	}
}

void RectangleSelection::end()
{
	if(!scene().selectionItem())
		return;

	// Remove tiny selections
	QRect sel = scene().selectionItem()->polygonRect();
	if(sel.width() * sel.height() <= 2)
		scene().setSelectionItem(0);
}

PolygonSelection::PolygonSelection(ToolCollection &owner)
	: Tool(owner, POLYGONSELECTION, QCursor(QPixmap(":cursors/select-lasso.png"), 2, 2))
{
}

void PolygonSelection::begin(const paintcore::Point &point, bool right, float zoom)
{
	// Right click to dismiss selection (and paste buffer)
	if(right) {
		scene().setSelectionItem(nullptr);
		return;
	}

	if(scene().selectionItem())
		_handle = scene().selectionItem()->handleAt(point.toPoint(), zoom);
	else
		_handle = drawingboard::SelectionItem::OUTSIDE;

	_start = point.toPoint();

	if(_handle == drawingboard::SelectionItem::OUTSIDE) {
		if(scene().selectionItem())
			scene().selectionItem()->pasteToCanvas(&client(), layer());

		scene().setSelectionItem(QPolygon({ point.toPoint() }));
	}
}

void PolygonSelection::motion(const paintcore::Point &point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);

	if(!scene().selectionItem())
		return;

	if(_handle==drawingboard::SelectionItem::OUTSIDE) {
		scene().selectionItem()->addPointToPolygon(point.toPoint());

	} else {
		// TODO constrain
		QPointF p = point - _start;
		scene().selectionItem()->adjustGeometry(_handle, p.toPoint());
		_start = point.toPoint();
	}
}

void PolygonSelection::end()
{
	if(!scene().selectionItem())
		return;

	scene().selectionItem()->closePolygon();

	// Remove tiny selections
	QRect sel = scene().selectionItem()->polygonRect();
	if(sel.width() * sel.height() <= 2)
		scene().setSelectionItem(nullptr);
}


}

