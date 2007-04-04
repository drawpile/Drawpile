/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

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

#include <QtGlobal>
#include <QPen>
#include "point.h"
#include "brush.h"
#include "preview.h"

namespace drawingboard {

//! Construct a stroke preview object
/**
 * @param parent parent layer
 * @param scene board to which this object belongs to
 */
StrokePreview::StrokePreview(QGraphicsItem *parent, QGraphicsScene *scene)
	: QGraphicsLineItem(parent, scene)
{
}

/**
 * @param from point from which the line begins
 * @param to point at which the line ends
 * @param brush brush to draw the line with
 */
void Preview::preview(const Point& from, const Point& to, const Brush& brush)
{
	brush_ = brush;
	from_ = from;
	to_ = to;
	const int rad = brush.radius(to.pressure());
	QColor color = brush.color(to.pressure());
	QPen pen;
	if(rad==0) {
		pen.setWidth(1);
		color.setAlphaF(brush.opacity(to.pressure()));
	} else {
		pen.setWidth(rad*2);
		pen.setCapStyle(Qt::RoundCap);
		pen.setJoinStyle(Qt::RoundJoin);
		// Approximate brush transparency
		const qreal a = brush.opacity(to.pressure()) * rad * (1-brush.spacing()/100.0);
		color.setAlphaF(qMin(a, 1.0));
	}
	pen.setColor(color);
	initAppearance(pen);
}

/**
 * @param to new endpoint
 */
void Preview::moveTo(const Point& to)
{
	to_ = to;
}

void StrokePreview::initAppearance(const QPen& pen)
{
	setPen(pen);
}

void StrokePreview::preview(const Point& from, const Point& to, const Brush& brush)
{
	Preview::preview(from, to, brush);

	setLine(from.x(), from.y(), to.x(), to.y());
	show();
}

void StrokePreview::moveTo(const Point& to)
{
	Preview::moveTo(to);
	setLine(from().x(), from().y(), to.x(), to.y());
}

RectanglePreview::RectanglePreview(QGraphicsItem *parent, QGraphicsScene *scene)
	: QGraphicsRectItem(parent, scene)
{
}

void RectanglePreview::initAppearance(const QPen& pen)
{
	setPen(pen);
}

void RectanglePreview::preview(const Point& from, const Point& to, const Brush& brush)
{
	Preview::preview(from, to, brush);

	qreal x,w;
	if(from.x() < to.x()) {
		x = from.x();
		w = to.x() - from.x();
	} else {
		x = to.x();
		w = from.x() - to.x();
	}
	qreal y,h;
	if(from.y() < to.y()) {
		y = from.y();
		h = to.y() - from.y();
	} else {
		y = to.y();
		h = from.y() - to.y();
	}
	setRect(x,y,w,h);
	show();
}

void RectanglePreview::moveTo(const Point& to)
{
	Preview::moveTo(to);
	qreal x,w;
	if(from().x() < to.x()) {
		x = from().x();
		w = to.x() - from().x();
	} else {
		x = to.x();
		w = from().x() - to.x();
	}
	qreal y,h;
	if(from().y() < to.y()) {
		y = from().y();
		h = to.y() - from().y();
	} else {
		y = to.y();
		h = from().y() - to.y();
	}
	setRect(x,y,w,h);
}

}

