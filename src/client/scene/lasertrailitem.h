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
#ifndef LASERTRAILITEM_H
#define LASERTRAILITEM_H

#include <QGraphicsLineItem>

namespace drawingboard {

class LaserTrailItem : public QGraphicsLineItem
{
public:
	LaserTrailItem(const QLineF &line, const QColor &color, int fadetime, QGraphicsItem *parent=0);

	/**
	 * @brief Advance fadeout animation
	 * @return true if the item just became completely transparent
	 */
	bool fadeoutStep(float dt);

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private:
	float _life;
	bool _blink;
	QPen _pen1, _pen2;
};

}

#endif // LASERTRAILITEM_H
