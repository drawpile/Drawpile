/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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
#include <QApplication>

// Qt 5.0 compatibility. Remove once Qt 5.1 ships on mainstream distros
#if (QT_VERSION < QT_VERSION_CHECK(5, 1, 0))
#include <cmath>
#define qAtan2 atan2
#else
#include <QtMath>
#endif

#include "tools.h"
#include "toolsettings.h"
#include "core/brush.h"
#include "canvasscene.h"
#include "annotationitem.h"
#include "selectionitem.h"

#include "net/client.h"
#include "net/utils.h"
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
	_tools[LASERPOINTER] = new LaserPointer(*this);
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

void BrushBase::begin(const paintcore::Point& point, bool right)
{
	const paintcore::Brush &brush = settings().getBrush(right);
	drawingboard::ToolContext tctx = {
		layer(),
		brush
	};

	if(!client().isLocalServer())
		scene().startPreview(brush, point);

	client().sendUndopoint();
	client().sendToolChange(tctx);
	client().sendStroke(point);
}

void BrushBase::motion(const paintcore::Point& point, bool constrain)
{
	Q_UNUSED(constrain);
	if(!client().isLocalServer())
		scene().addPreview(point);

	client().sendStroke(point);
}

void BrushBase::end()
{
	client().sendPenup();
}

void LaserPointer::begin(const paintcore::Point &point, bool right)
{
	Q_UNUSED(right);
	// Send initial point to serve as the start of the line,
	// and also a toolchange to set the laser color

	const paintcore::Brush &brush = settings().getBrush(right);
	drawingboard::ToolContext tctx = {
		layer(),
		brush
	};
	client().sendToolChange(tctx);
	client().sendLaserPointer(point.x(), point.y(), 0);
}

void LaserPointer::motion(const paintcore::Point &point, bool constrain)
{
	Q_UNUSED(constrain);
	client().sendLaserPointer(point.x(), point.y(), settings().getLaserPointerSettings()->trailPersistence());
}

void LaserPointer::end()
{
	// Nothing to do here, laser trail does not need penup type notification
}

void ColorPicker::begin(const paintcore::Point& point, bool right)
{
	_bg = right;
	motion(point, false);
}

void ColorPicker::motion(const paintcore::Point& point, bool constrain)
{
	Q_UNUSED(constrain);
	int layer=0;
	if(settings().getColorPickerSettings()->pickFromLayer()) {
		layer = this->layer();
	}
	scene().pickColor(point.x(), point.y(), layer, _bg);
}

void ColorPicker::end()
{
}

void Line::begin(const paintcore::Point& point, bool right)
{
	QGraphicsLineItem *item = new QGraphicsLineItem();
	item->setPen(drawingboard::CanvasScene::penForBrush(settings().getBrush(right)));
	item->setLine(QLineF(point, point));
	scene().setToolPreview(item);
	_p1 = point;
	_p2 = point;
	_swap = right;
}

namespace {
QPointF angleConstraint(const QPointF &p1, const QPointF &p2)
{
	QPointF dp = p2 - p1;
	double a = qAtan2(dp.y(), dp.x());
	double m = hypot(dp.x(), dp.y());

	// Round a to the nearest multiple of π/4
	const double STEPS = M_PI / 4.0;
	a = qRound(a / STEPS) * STEPS;

	return p1 + QPointF(cos(a)*m, sin(a)*m);
}
}

void Line::motion(const paintcore::Point& point, bool constrain)
{
	if(constrain)
		_p2 = angleConstraint(_p1, point);
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

void Rectangle::begin(const paintcore::Point& point, bool right)
{
	QGraphicsRectItem *item = new QGraphicsRectItem();
	item->setPen(drawingboard::CanvasScene::penForBrush(settings().getBrush(right)));
	item->setRect(QRectF(point, point));
	scene().setToolPreview(item);
	_p1 = point;
	_p2 = point;
	_swap = right;
}

namespace {

QPointF squareConstraint(const QPointF &p1, const QPointF &p2)
{
	float dx = p2.x() - p1.x();
	float dy = p2.y() - p1.y();
	const float ax = qAbs(dx);
	const float ay = qAbs(dy);

	if(ax>ay)
		dy = ax * (dy<0?-1:1);
	else
		dx = ay * (dx<0?-1:1);

	return p1 + QPointF(dx, dy);
}

}

void Rectangle::motion(const paintcore::Point& point, bool constrain)
{
	if(constrain)
		_p2 = squareConstraint(_p1, point);
	else
		_p2 = point;

	QGraphicsRectItem *item = qgraphicsitem_cast<QGraphicsRectItem*>(scene().toolPreview());
	if(item)
		item->setRect(QRectF(_p1, _p2).normalized());
}

void Rectangle::end()
{
	using namespace paintcore;
	scene().setToolPreview(0);

	drawingboard::ToolContext tctx = {
		layer(),
		settings().getBrush(_swap)
	};

	client().sendUndopoint();
	client().sendToolChange(tctx);
	PointVector pv;
	pv << Point(_p1, 1);
	pv << Point(_p1.x(), _p2.y(), 1);
	pv << Point(_p2, 1);
	pv << Point(_p2.x(), _p1.y(), 1);
	pv << Point(_p1 - QPointF(_p1.x()<_p2.x()?-1:1, 0), 1);
	client().sendStroke(pv);
	client().sendPenup();
}

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
		_end = point;
		_wasselected = false;
	}
	_start = point;
}

/**
 * If we have a selected annotation, move or resize it. Otherwise extend
 * the preview rectangle for the new annotation.
 */
void Annotation::motion(const paintcore::Point& point, bool constrain)
{
	if(_wasselected) {
		// TODO a "ghost" mode to indicate annotation has not really moved
		// until the server roundtrip

		// Annotation may have been deleted by other user while we were moving it.
		if(_selected) {
			QPointF p = point - _start;
			// TODO constrain
			_selected->adjustGeometry(_handle, p.toPoint());
			_start = point;
		}
	} else {
		if(constrain)
			_end = squareConstraint(_start, point);
		else
			_end = point;

		QGraphicsRectItem *item = qgraphicsitem_cast<QGraphicsRectItem*>(scene().toolPreview());
		if(item)
			item->setRect(QRectF(_start, _end).normalized());
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
			client().sendAnnotationReshape(_selected->id(), _selected->geometry());
			_selected = 0;
		}
	} else {
		scene().setToolPreview(0);

		QRect rect = QRect(_start.toPoint(), _end.toPoint()).normalized();

		if(rect.width()<15)
			rect.setWidth(15);
		if(rect.height()<15)
			rect.setHeight(15);

		client().sendAnnotationCreate(0, rect);
	}
}

void Selection::begin(const paintcore::Point &point, bool right)
{
	// Right click to dismiss selection (and paste buffer)
	if(right) {
		scene().setSelectionItem(0);
		return;
	}

	if(scene().selectionItem())
		_handle = scene().selectionItem()->handleAt(point.toPoint());
	else
		_handle = drawingboard::SelectionItem::OUTSIDE;

	_start = point.toPoint();
	if(_handle == drawingboard::SelectionItem::OUTSIDE) {
		bool hasPaste = scene().selectionItem() && !scene().selectionItem()->pasteImage().isNull();
		if(hasPaste) {
			// Left click outside and paste buffer exists: merge image
			QImage image = scene().selectionItem()->pasteImage();
			const QRect rect = scene().selectionItem()->rect();
			if(image.size() != rect.size())
				image = image.scaled(rect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

			// Clip image to scene
			const QRect scenerect(0, 0, scene().width(), scene().height());
			QRect intersection = rect & scenerect;
			if(!intersection.isEmpty()) {
				int xoff=0, yoff=0;
				if(intersection != rect) {
					if(rect.x() < 0)
						xoff = -rect.x();
					if(rect.y() < 0)
						yoff = -rect.y();

					intersection.moveLeft(xoff);
					intersection.moveTop(yoff);
					image = image.copy(intersection);
				}

				// Merge image
				client().sendUndopoint();
				client().sendImage(layer(), rect.x() + xoff, rect.y() + yoff, image, true);
			}
			scene().setSelectionItem(0);
		} else {
			drawingboard::SelectionItem *sel = new drawingboard::SelectionItem();
			sel->setRect(QRectF(point, point).toRect());
			scene().setSelectionItem(sel);
		}
	}
}

void Selection::motion(const paintcore::Point &point, bool constrain)
{
	if(!scene().selectionItem())
		return;

	if(_handle==drawingboard::SelectionItem::OUTSIDE) {
		QPointF p;
		if(constrain)
			p = squareConstraint(_start, point);
		else
			p = point;

		scene().selectionItem()->setRect(QRectF(_start, p).normalized().toRect());
	} else {
			// TODO constrain
		QPointF p = point - _start;
		scene().selectionItem()->adjustGeometry(_handle, p.toPoint());
		_start = point.toPoint();
	}
}

void Selection::end()
{
	if(!scene().selectionItem())
		return;

	// Remove tiny selections
	QRect sel = scene().selectionItem()->rect();
	if(sel.width() * sel.height() <= 2)
		scene().setSelectionItem(0);
}

}
