/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#ifndef LASERTRAILITEM_H
#define LASERTRAILITEM_H

#include <QGraphicsLineItem>
#include <QPen>

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
