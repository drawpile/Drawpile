/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include <QPen>
#include <QPainter>

#include "lasertrailitem.h"

namespace drawingboard {

LaserTrailItem::LaserTrailItem(const QLineF &line, const QColor &color, int fadetime, QGraphicsItem *parent)
	: QGraphicsLineItem(line, parent), _blink(false)
{
	_pen1.setCosmetic(true);
	_pen1.setWidth(1);
	_pen1.setColor(color.lighter(110));

	_pen2 = _pen1;
	_pen2.setColor(color.lighter(90));

	_life = fadetime;
}

void LaserTrailItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	Q_UNUSED(option);
	Q_UNUSED(widget);
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, true);
	painter->setPen(_blink ? _pen1 : _pen2);
	//if(_blink)
		//painter->setCompositionMode(QPainter::CompositionMode_Plus);
	painter->drawLine(line());
	painter->restore();
}

bool LaserTrailItem::fadeoutStep(float dt)
{
	_blink = !_blink;
	_life -= dt;
	if(_life<=0.0)
		return true;

	if(_life<1.0)
		setOpacity(_life);

	update(boundingRect());
	return false;
}

}
