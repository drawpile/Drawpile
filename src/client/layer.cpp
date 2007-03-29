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

#include "point.h"
#include "layer.h"
#include "brush.h"

namespace drawingboard {

/**
 * @param parent use another QGraphicsItem as a parent
 * @param scene the picture to which this layer belongs to
 */
Layer::Layer(QGraphicsItem *parent, QGraphicsScene *scene)
	: QGraphicsItem(parent,scene)
{
}

/**
 * @param image image to use
 * @param parent use another QGraphicsItem as a parent
 * @param scene the picture to which this layer belongs to
 */
Layer::Layer(const QImage& image, QGraphicsItem *parent, QGraphicsScene *scene)
	: QGraphicsItem(parent,scene), image_(image)
{
	Q_ASSERT(image_.format() == QImage::Format_RGB32 || image_.format() == QImage::Format_ARGB32);
}

/**
 * @param image image to use
 */
void Layer::setImage(const QImage& image)
{
	image_ = image;
	Q_ASSERT(image_.format() == QImage::Format_RGB32 || image_.format() == QImage::Format_ARGB32);
}

/**
 * @return layer contents
 */
QImage Layer::image() const
{
	return image_;
}

/**
 * The brush pixmap is drawn at each point between point1 and point2.
 * Pressure values are interpolated between the points.
 * First pixel is not drawn. This is done on purpose, as drawLine is usually
 * used to draw multiple joined lines.
 *
 * If distance is not null, it is used to add spacing between dabs.
 * @param point1 start coordinates
 * @param point2 end coordinates
 * @param brush brush to draw with
 * @param distance total drawn line length
 */
void Layer::drawLine(const Point& point1, const Point& point2, const Brush& brush,int *distance)
{
#if 0 // TODO
	qreal pressure = point1.pressure();
	qreal deltapressure;
	if(qAbs(pressure2-pressure1) < 1.0/255.0)
		deltapressure = 0;
	else
		deltapressure = (pressure2-pressure1) / hypot(point1.x()-point2.x(), point1.y()-point2.y());
#endif

	const int spacing = brush.spacing()*brush.radius(point1.pressure())/100;

	Point point = point1;
	int &x0 = point.rx();
	int &y0 = point.ry();
	int x1 = point2.x();
	int y1 = point2.y();
	int dy = y1 - y0;
	int dx = x1 - x0;
	int stepx, stepy;

	if (dy < 0) {
		dy = -dy;
		stepy = -1;
	} else {
		stepy = 1;
	}
	if (dx < 0) {
		dx = -dx;
		stepx = -1;
	} else {
		stepx = 1;
	}

	dy *= 2;
	dx *= 2;

	if (dx > dy) {
		int fraction = dy - (dx >> 1);
		while (x0 != x1) {
			if (fraction >= 0) {
				y0 += stepy;
				fraction -= dx;
			}
			x0 += stepx;
			fraction += dy;
			if(distance) {
				if(++*distance > spacing) {
					brush.draw(image_, point);
					*distance = 0;
				}
			} else {
				brush.draw(image_, point);
			}
		}
	} else {
		int fraction = dx - (dy >> 1);
		while (y0 != y1) {
			if (fraction >= 0) {
				x0 += stepx;
				fraction -= dy;
			}
			y0 += stepy;
			fraction += dx;
			if(distance) {
				if(++*distance > spacing) {
					brush.draw(image_, point);
					*distance = 0;
				}
			} else {
				brush.draw(image_, point);
			}
		}
	}
	// Update screen
	const int left = qMin(point1.x(), point2.x());
	const int right = qMax(point1.x(), point2.x());
	const int top = qMin(point1.y(), point2.y());
	const int bottom = qMax(point1.y(), point2.y());
	int rad = brush.radius(point1.pressure());
	if(rad==0) rad=1;
	update(left-rad,top-rad,right-left+rad*2,bottom-top+rad*2);
}

/**
 * @param point coordinates
 * @param brush brush to use
 */
void Layer::drawPoint(const Point& point, const Brush& brush)
{
	int r = brush.radius(point.pressure());
	if(r==0) r=1;
	brush.draw(image_, point);
	update(point.x()-r,point.y()-r,r*2,r*2);
}

QRectF Layer::boundingRect() const
{
	return QRectF(0,0, image_.width(), image_.height());
}

void Layer::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
	 QWidget *)
{
	QRectF exposed = option->exposedRect.adjusted(-1, -1, 1, 1);
	exposed &= QRectF(0,0,image_.width(),image_.height());
	painter->drawImage(exposed, image_, exposed);
}

}

