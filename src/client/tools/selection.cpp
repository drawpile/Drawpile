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

#include <cmath>

namespace tools {

void SelectionTool::begin(const paintcore::Point &point, bool right, float zoom)
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

		initSelection();
	}
}

void SelectionTool::motion(const paintcore::Point &point, bool constrain, bool center)
{
	if(!scene().selectionItem())
		return;

	if(_handle==drawingboard::SelectionItem::OUTSIDE) {
		newSelectionMotion(point, constrain, center);

	} else {
		QPointF p = point - _start;

		if(scene().selectionItem()->pasteImage().isNull() && !scene().statetracker()->isLayerLocked(layer())) {
			// Automatically cut the layer when the selection is transformed
			QImage img = scene().selectionToImage(layer());
			scene().selectionItem()->fillCanvas(Qt::transparent, &client(), layer());
			scene().selectionItem()->setPasteImage(img);
		}

		if(_handle == drawingboard::SelectionItem::TRANSLATE && center) {
			// We use the center constraint during translation to rotate the selection
			const QPoint center = scene().selectionItem()->polygonRect().center();

			double a0 = atan2(_start.y() - center.y(), _start.x() - center.x());
			double a1 = atan2(point.y() - center.y(), point.x() - center.x());

			scene().selectionItem()->rotate(a1-a0);

		} else {
			// TODO constraints

			scene().selectionItem()->adjustGeometry(_handle, p.toPoint());
		}

		_start = point.toPoint();
	}
}

void SelectionTool::end()
{
	if(!scene().selectionItem())
		return;

	// Remove tiny selections
	QRect sel = scene().selectionItem()->polygonRect();
	if(sel.width() * sel.height() <= 2)
		scene().setSelectionItem(0);
}

RectangleSelection::RectangleSelection(ToolCollection &owner)
	: SelectionTool(owner, SELECTION, QCursor(QPixmap(":cursors/select-rectangle.png"), 2, 2))
{
}

void RectangleSelection::initSelection()
{
	scene().setSelectionItem(QRect(_start, _start));
}

void RectangleSelection::newSelectionMotion(const paintcore::Point& point, bool constrain, bool center)
{
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
}

PolygonSelection::PolygonSelection(ToolCollection &owner)
	: SelectionTool(owner, POLYGONSELECTION, QCursor(QPixmap(":cursors/select-lasso.png"), 2, 2))
{
}

void PolygonSelection::initSelection()
{
	scene().setSelectionItem(QPolygon({ _start }));
}

void PolygonSelection::newSelectionMotion(const paintcore::Point &point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);

	scene().selectionItem()->addPointToPolygon(point.toPoint());
}

void PolygonSelection::end()
{
	if(scene().selectionItem())
		scene().selectionItem()->closePolygon();

	SelectionTool::end();
}


}

