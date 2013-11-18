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

namespace {

/**
 * @brief Convert a brush definition to a QPen
 * This is used to set preview stroke styles.
 * @param brush
 * @return QPen instance
 */
QPen brush2pen(const dpcore::Brush &brush)
{
	const int rad = brush.radius(1.0);
	QColor color = brush.color(1.0);
	QPen pen;
	if(rad==0) {
		pen.setWidth(1);
		color.setAlphaF(brush.opacity(1.0));
	} else {
		pen.setWidth(rad*2);
		pen.setCapStyle(Qt::RoundCap);
		pen.setJoinStyle(Qt::RoundJoin);
		// Approximate brush transparency
		const qreal a = brush.opacity(1.0) * rad * (1-brush.spacing()/100.0);
		color.setAlphaF(qMin(a, 1.0));
	}
	pen.setColor(color);
	return pen;
}

}

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

void ToolCollection::setToolSettings(widgets::ToolSettings *s)
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

void BrushBase::begin(const dpcore::Point& point)
{
	drawingboard::ToolContext tctx = {
		layer(),
		settings().getBrush()
	};
	
	client().sendToolChange(tctx);
	client().sendStroke(point);
}

void BrushBase::motion(const dpcore::Point& point)
{
	client().sendStroke(point);
}

void BrushBase::end()
{
	client().sendPenup();
}

void ColorPicker::begin(const dpcore::Point& point)
{
	scene().pickColor(point.x(), point.y());
}

void ColorPicker::motion(const dpcore::Point& point)
{
	scene().pickColor(point.x(), point.y());
}

void ColorPicker::end()
{
}

void Line::begin(const dpcore::Point& point)
{
	QGraphicsLineItem *item = new QGraphicsLineItem();
	item->setPen(brush2pen(settings().getBrush()));
	item->setLine(QLineF(point, point));
	scene().setToolPreview(item);
	_p1 = point;
	_p2 = point;
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
		settings().getBrush()
	};

	client().sendToolChange(tctx);
	PointVector pv;
	pv << _p1 << _p2;
	client().sendStroke(pv);
	client().sendPenup();
}

void Rectangle::begin(const dpcore::Point& point)
{
	QGraphicsRectItem *item = new QGraphicsRectItem();
	item->setPen(brush2pen(settings().getBrush()));
	item->setRect(QRectF(point, point));
	scene().setToolPreview(item);
	_p1 = point;
	_p2 = point;
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
		settings().getBrush()
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
void Annotation::begin(const dpcore::Point& point)
{
#if 0
	drawingboard::AnnotationItem *item = editor()->annotationAt(point);
	if(item) {
		sel_ = item;
		handle_ = sel_->handleAt(point - sel_->pos());
		aeditor()->setSelection(item);
	} else {
		editor()->startPreview(ANNOTATION, point, editor()->localBrush());
		end_ = point;
	}
	start_ = point;
#endif
}

/**
 * If we have a selected annotation, move or resize it. Otherwise extend
 * the preview rectangle for the new annotation.
 */
void Annotation::motion(const dpcore::Point& point)
{
#if 0
	if(sel_) {
		QPoint d = point - start_;
		switch(handle_) {
			case drawingboard::AnnotationItem::TRANSLATE:
				sel_->moveBy(d.x(), d.y());
				break;
			case drawingboard::AnnotationItem::RS_TOPLEFT:
				sel_->growTopLeft(d.x(), d.y());
				break;
			case drawingboard::AnnotationItem::RS_BOTTOMRIGHT:
				sel_->growBottomRight(d.x(), d.y());
				break;
		}
		start_ = point;
	} else {
		editor()->continuePreview(point);
		end_ = point;
	}
#endif
}

/**
 * If we have a selection, reannotate it. This is does nothing in local mode,
 * but is needed in a network session to inform the server of the new size/
 * position. (Room for optimization, don't reannotate if geometry didn't change)
 * If no existing annotation was selected, create a new one based on the
 * starting and ending coordinates. The new annotation is given some minimum
 * size to make sure something appears when the user just clicks and doesn't
 * move the mouse/stylus.
 */
void Annotation::end()
{
#if 0
	if(sel_) {
		// This is superfluous when in local mode, but needed
		// when in a networked session.
		// TODO
		protocol::Annotation a;
		sel_->getOptions(a);
		editor()->annotate(a);
		sel_ = 0;
	} else {
		editor()->endPreview();
		QRectF rect(QRectF(start_, end_));
		rect = rect.normalized();
		if(rect.width()<15)
			rect.setWidth(15);
		if(rect.height()<15)
			rect.setHeight(15);
		protocol::Annotation a;
		a.rect = rect.toRect();
		a.textcolor = editor()->localBrush().color(1.0).name();
		a.backgroundcolor = editor()->localBrush().color(0.0).name();
		editor()->annotate(a);
	}
#endif
}

}

