/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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

#include <QPainter>
#include <QGraphicsItem>
#include <QStyleOptionGraphicsItem>
#include <cmath>

#include "layer.h"
#include "brush.h"

namespace drawingboard {

Layer::Layer(QGraphicsItem *parent, QGraphicsScene *scene)
	: QGraphicsItem(parent,scene)
{
}

Layer::Layer(const QImage& image, QGraphicsItem *parent, QGraphicsScene *scene)
	: QGraphicsItem(parent,scene), image_(image)
{
}

void Layer::setImage(const QImage& image)
{
	image_ = image;
}

QImage Layer::image() const
{
	return image_;
}

/**
 * The brush pixmap is drawn at each point between point1 and point2.
 * Pressure values are interpolated between the points.
 */
void Layer::drawLine(const QPoint& point1, qreal pressure1,
		const QPoint& point2, qreal pressure2, const Brush& brush)
{
	QPainter painter(&image_);
	painter.setOpacity(1.0);
	qreal pressure = pressure1;
	qreal deltapressure;
	if(pressure2-pressure1 < 1.0/255.0)
		deltapressure = 0;
	else
		deltapressure = (pressure2-pressure1) / hypot(point1.x()-point2.x(), point1.y()-point2.y());

	// Bresenham's line drawing algorithm
	bool steep = abs(point2.y() - point1.y()) > abs(point2.x() - point1.x());
	int x1,x2,y1,y2;
	if(steep) {
		x1 = point1.y(); y1 = point1.x();
		x2 = point2.y(); y2 = point2.x();
	} else {
		x1 = point1.x(); y1 = point1.y();
		x2 = point2.x(); y2 = point2.y();
	}
	if(x1 > x2) {
		int tmp;
		tmp = x1; x1 = x2; x2 = tmp;
		tmp = y1; y1 = y2; y2 = tmp;
	}
	int deltax = x2-x1;
	int deltay = abs(y2-y1);
	int error = 0;
	int ystep = y1<y2?1:-1;
	int y = y1;
	for(int x=x1;x<x2;++x) {
		if(steep)
			drawPoint(painter,y,x,pressure1,brush);
		else
			drawPoint(painter,x,y,pressure1,brush);
		pressure += deltapressure;
		error += deltay;
		if(2*error >= deltax) {
			y += ystep;
			error -= deltax;
		}
	}

	// Update screen
	x1 = qMin(point1.x(), point2.x());
	x2 = qMax(point1.x(), point2.x());
	y1 = qMin(point1.y(), point2.y());
	y2 = qMax(point1.y(), point2.y());
	int rad = qMax(brush.radius(pressure1),brush.radius(pressure2));
	update(x1-rad,y1-rad,x2-x1+rad*2,y2-y1+rad*2);
}

/**
 * This function sets up a painting environment and calls the internal drawPoint()
 * @param point coordinates
 * @param pressure pen pressure
 * @param brush brush to use
 */
void Layer::drawPoint(const QPoint& point, qreal pressure, const Brush& brush)
{
	int r = brush.radius(pressure);
	QPainter painter(&image_);
	drawPoint(painter,point.x(),point.y(), pressure, brush);
	update(point.x()-r,point.y()-r,point.x()+r,point.y()+r);
}

/**
 * Prepare painter and draw a single dot with the brush. The dot will be
 * drawn centered at the x and y coordinates. If brush diameter is 1,
 * the dot will be drawn with QPainters drawPoint().
 * @param painter painter to use
 * @param x center x coordinate
 * @param y center y coordinate
 * @param pressure pen pressure
 * @param brush brush to use
 */
void Layer::drawPoint(QPainter &painter, int x,int y, qreal pressure, const Brush& brush)
{
	int r = brush.radius(pressure);
	QPoint p(x-r,y-r);
	if(r==0) {
		painter.setOpacity(brush.opacity(pressure));
		painter.setPen(brush.color(pressure));
		painter.drawPoint(p);
	} else {
		painter.drawImage(p, brush.getBrush(pressure));
	}

}

QRectF Layer::boundingRect() const
{
	return QRectF(0,0, image_.width(), image_.height());
}

void Layer::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
	 QWidget *widget)
{
	QRectF exposed = option->exposedRect.adjusted(-1, -1, 1, 1);
    painter->drawImage(exposed, image_, exposed);
}

}

