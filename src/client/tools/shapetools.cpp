/*
   Drawpile - a collaborative drawing program.

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

#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>

#include "scene/canvasscene.h"
#include "docks/toolsettingsdock.h"
#include "core/shapes.h"
#include "net/client.h"
#include "statetracker.h"

#include "tools/shapetools.h"
#include "tools/utils.h"

namespace tools {

void Line::begin(const paintcore::Point& point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	QGraphicsLineItem *item = new QGraphicsLineItem();
	item->setPen(drawingboard::CanvasScene::penForBrush(settings().getBrush(right)));
	item->setLine(QLineF(point, point));
	scene().setToolPreview(item);
	_p1 = point;
	_p2 = point;
	_swap = right;
}

void Line::motion(const paintcore::Point& point, bool constrain, bool center)
{
	Q_UNUSED(center);
	if(constrain)
		_p2 = constraints::angle(_p1, point);
	else
		_p2 = point;

	QGraphicsLineItem *item = qgraphicsitem_cast<QGraphicsLineItem*>(scene().toolPreview());
	if(item)
		item->setLine(QLineF(_p1, _p2));
}

void Line::end()
{
	using namespace paintcore;
	scene().setToolPreview(0);

	drawingboard::ToolContext tctx = {
		layer(),
		settings().getBrush(_swap)
	};

	client().sendUndopoint();
	client().sendToolChange(tctx);
	client().sendStroke(PointVector() << Point(_p1, 1) << Point(_p2, 1));
	client().sendPenup();
}

void RectangularTool::begin(const paintcore::Point& point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	auto *item = createPreview(point);
	item->setPen(drawingboard::CanvasScene::penForBrush(settings().getBrush(right)));
	scene().setToolPreview(item);
	_start = point;
	_p1 = point;
	_p2 = point;
	_swap = right;
}

void RectangularTool::motion(const paintcore::Point& point, bool constrain, bool center)
{
	if(constrain)
		_p2 = constraints::square(_start, point);
	else
		_p2 = point;

	if(center)
		_p1 = _start - (_p2 - _start);
	else
		_p1 = _start;

	updateToolPreview();
}

void RectangularTool::end()
{
	scene().setToolPreview(0);

	drawingboard::ToolContext tctx = {
		layer(),
		settings().getBrush(_swap)
	};

	client().sendUndopoint();
	client().sendToolChange(tctx);
	client().sendStroke(pointVector());
	client().sendPenup();
}

Rectangle::Rectangle(ToolCollection &owner)
	: RectangularTool(owner, RECTANGLE, QCursor(QPixmap(":cursors/rectangle.png"), 2, 2))
{
}

QAbstractGraphicsShapeItem *Rectangle::createPreview(const paintcore::Point &p)
{
	return new QGraphicsRectItem(p.x(), p.y(), 1, 1, 0);
}

void Rectangle::updateToolPreview()
{
	auto *item = qgraphicsitem_cast<QGraphicsRectItem*>(scene().toolPreview());
	if(item)
		item->setRect(rect());
}

paintcore::PointVector Rectangle::pointVector()
{
	return paintcore::shapes::rectangle(rect());
}

Ellipse::Ellipse(ToolCollection &owner)
	: RectangularTool(owner, ELLIPSE, QCursor(QPixmap(":cursors/ellipse.png"), 2, 2))
{
}

QAbstractGraphicsShapeItem *Ellipse::createPreview(const paintcore::Point &p)
{
	return new QGraphicsEllipseItem(p.x(), p.y(), 1, 1, 0);
}

void Ellipse::updateToolPreview()
{
	auto *item = qgraphicsitem_cast<QGraphicsEllipseItem*>(scene().toolPreview());
	if(item)
		item->setRect(rect());
}

paintcore::PointVector Ellipse::pointVector()
{
	return paintcore::shapes::ellipse(rect());
}

}

