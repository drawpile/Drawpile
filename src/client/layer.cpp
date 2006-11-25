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

#include "layer.h"
#include "brush.h"

namespace drawingboard {

Layer::Layer(QGraphicsItem *parent, QGraphicsScene *scene)
	: QGraphicsItem(parent,scene)
{
}

Layer::Layer(const QPixmap& pixmap, QGraphicsItem *parent, QGraphicsScene *scene)
	: QGraphicsItem(parent,scene), pixmap_(pixmap)
{
}

void Layer::setPixmap(const QPixmap& pixmap)
{
	pixmap_ = pixmap;
}

QPixmap Layer::pixmap() const
{
	return pixmap_;
}

void Layer::drawLine(const QLine& line, const Brush& brush)
{
	QPainter painter(&pixmap_);
	QPixmap b = brush.getBrush(1.0);
	painter.drawPixmap(line.p1(), b);
	update(line.x1(),line.y1(),line.x1()+b.width(),line.y1()+b.height());
	//painter.drawLine(line);

	//update(line.x1(),line.y1(), line.x2(), line.y2());
}

QRectF Layer::boundingRect() const
{
	return QRectF(0,0, pixmap_.width(), pixmap_.height());
}

void Layer::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
	 QWidget *widget)
{
	QRect rect = option->exposedRect.toRect();
	painter->drawPixmap(rect, pixmap_, rect);
}

}

