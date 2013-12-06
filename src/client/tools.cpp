/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <QDebug>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>

#include "tools.h"
#include "toolsettings.h"
#include "core/brush.h"
#include "canvasscene.h"
#include "annotationitem.h"

#include "net/client.h"
#include "docks/toolsettingswidget.h"
#include "statetracker.h"

namespace tools {

/**
 * Construct one of each tool for the collection
 */
ToolCollection::ToolCollection()
	: _client(0), _toolsettings(0)
{
	_tools[PEN] = new Pen(*this);
	_tools[BRUSH] = new Brush(*this);
	_tools[ERASER] = new Eraser(*this);
	_tools[PICKER] = new ColorPicker(*this);
	_tools[LINE] = new Line(*this);
	_tools[RECTANGLE] = new Rectangle(*this);
	_tools[ANNOTATION] = new Annotation(*this);
	_tools[SELECTION] = new Selection(*this);
}

/**
 * Delete the tools
 */
ToolCollection::~ToolCollection()
{
	foreach(Tool *t, _tools)
		delete t;
}

void ToolCollection::setClient(net::Client *client)
{
	Q_ASSERT(client);
	_client = client;
}

void ToolCollection::setScene(drawingboard::CanvasScene *scene)
{
    Q_ASSERT(scene);
    _scene = scene;
}

void ToolCollection::setToolSettings(widgets::ToolSettingsDock *s)
{
	Q_ASSERT(s);
	_toolsettings = s;
}

void ToolCollection::selectLayer(int layer_id)
{
	_layer = layer_id;
}

/**
 * The returned tool can be used to perform actions on the board
 * controlled by the specified controller.
 * 
 * @param type type of tool wanted
 * @return the requested tool
 */
Tool *ToolCollection::get(Type type)
{
	Q_ASSERT(_tools.contains(type));
	return _tools.value(type);
}

void BrushBase::begin(const dpcore::Point& point, bool right)
{
	const dpcore::Brush &brush = settings().getBrush(right);
	drawingboard::ToolContext tctx = {
		layer(),
		brush
	};

	if(!client().isLocalServer())
		scene().startPreview(brush, point);

	client().sendToolChange(tctx);
	client().sendStroke(point);
}

void BrushBase::motion(const dpcore::Point& point)
{
	if(!client().isLocalServer())
		scene().addPreview(point);

	client().sendStroke(point);
}

void BrushBase::end()
{
	client().sendPenup();
}

void ColorPicker::begin(const dpcore::Point& point, bool right)
{
	Q_UNUSED(right);
	scene().pickColor(point.x(), point.y());
}

void ColorPicker::motion(const dpcore::Point& point)
{
	scene().pickColor(point.x(), point.y());
}

void ColorPicker::end()
{
}

void Line::begin(const dpcore::Point& point, bool right)
{
	QGraphicsLineItem *item = new QGraphicsLineItem();
	item->setPen(drawingboard::CanvasScene::penForBrush(settings().getBrush(right)));
	item->setLine(QLineF(point, point));
	scene().setToolPreview(item);
	_p1 = point;
	_p2 = point;
	_swap = right;
}

void Line::motion(const dpcore::Point& point)
{
	_p2 = point;
	QGraphicsLineItem *item = qgraphicsitem_cast<QGraphicsLineItem*>(scene().toolPreview());
	if(item)
		item->setLine(QLineF(_p1, _p2));
}

void Line::end()
{
	using namespace dpcore;
	scene().setToolPreview(0);

	drawingboard::ToolContext tctx = {
		layer(),
		settings().getBrush(_swap)
	};

	client().sendToolChange(tctx);
	PointVector pv;
	pv << _p1 << _p2;
	client().sendStroke(pv);
	client().sendPenup();
}

void Rectangle::begin(const dpcore::Point& point, bool right)
{
	QGraphicsRectItem *item = new QGraphicsRectItem();
	item->setPen(drawingboard::CanvasScene::penForBrush(settings().getBrush(right)));
	item->setRect(QRectF(point, point));
	scene().setToolPreview(item);
	_p1 = point;
	_p2 = point;
	_swap = right;
}

void Rectangle::motion(const dpcore::Point& point)
{
	_p2 = point;
	QGraphicsRectItem *item = qgraphicsitem_cast<QGraphicsRectItem*>(scene().toolPreview());
	if(item)
		item->setRect(QRectF(_p1, _p2).normalized());
}

void Rectangle::end()
{
	using namespace dpcore;
	scene().setToolPreview(0);

	drawingboard::ToolContext tctx = {
		layer(),
		settings().getBrush(_swap)
	};

	client().sendToolChange(tctx);
	PointVector pv;
	pv << _p1;
	pv << Point(_p1.x(), _p2.y(), _p1.pressure());
	pv << _p2;
	pv << Point(_p2.x(), _p1.y(), _p1.pressure());
	pv << _p1 - Point(_p1.x()<_p2.x()?-1:1,0,1);
	client().sendStroke(pv);
	client().sendPenup();
}

/**
 * The annotation tool has fairly complex needs. Clicking on an existing
 * annotation selects it, otherwise a new annotation is started.
 */
void Annotation::begin(const dpcore::Point& point, bool right)
{
	Q_UNUSED(right);

	drawingboard::AnnotationItem *item = scene().annotationAt(point);
	if(item) {
		_selected = item;
		_handle = _selected->handleAt(point - _selected->pos());
		settings().getAnnotationSettings()->setSelection(item);
	} else {
		QGraphicsRectItem *item = new QGraphicsRectItem();
		QPen pen;
		pen.setWidth(1);
		pen.setColor(Qt::red); // TODO
		pen.setStyle(Qt::DotLine);
		item->setPen(pen);
		item->setRect(QRectF(point, point));
		scene().setToolPreview(item);
		_end = point;
	}
	_start = point;
}

/**
 * If we have a selected annotation, move or resize it. Otherwise extend
 * the preview rectangle for the new annotation.
 */
void Annotation::motion(const dpcore::Point& point)
{
	if(_selected) {
		// TODO a "ghost" mode to indicate annotation has not really moved
		// until the server roundtrip
		QPoint d = point - _start;
		switch(_handle) {
			case drawingboard::AnnotationItem::TRANSLATE:
				_selected->moveBy(d.x(), d.y());
				break;
			case drawingboard::AnnotationItem::RS_TOPLEFT:
				_selected->growTopLeft(d.x(), d.y());
				break;
			case drawingboard::AnnotationItem::RS_BOTTOMRIGHT:
				_selected->growBottomRight(d.x(), d.y());
				break;
		}
		_start = point;
	} else {
		QGraphicsRectItem *item = qgraphicsitem_cast<QGraphicsRectItem*>(scene().toolPreview());
		if(item)
			item->setRect(QRectF(_start, _end).normalized());
		_end = point;
	}
}

/**
 * If we have a selected annotation, adjust its shape.
 * Otherwise, create a new annotation.
 */
void Annotation::end()
{
	if(_selected) {
		client().sendAnnotationReshape(_selected->id(), _selected->geometry());
		_selected = 0;
	} else {
		scene().setToolPreview(0);

		QRect rect = QRect(_start, _end).normalized();

		if(rect.width()<15)
			rect.setWidth(15);
		if(rect.height()<15)
			rect.setHeight(15);

		client().sendAnnotationCreate(0, rect);
	}
}

void Selection::begin(const dpcore::Point &point, bool right)
{

}

void Selection::motion(const dpcore::Point &point)
{

}

void Selection::end()
{

}

}
