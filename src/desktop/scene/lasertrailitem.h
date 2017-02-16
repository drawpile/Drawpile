/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2017 Calle Laakkonen

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

#include <QGraphicsItem>
#include <QPen>

namespace drawingboard {

class LaserTrailItem : public QGraphicsItem
{
public:
	LaserTrailItem(QGraphicsItem *parent=nullptr);

	void animationStep(float dt);

	void setPoints(const QVector<QPointF> &points);
	void setColor(const QColor &color);
	void setFadeVisible(bool visible);

	QRectF boundingRect() const;
protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private:
	bool m_blink;
	bool m_visible;
	QPen m_pen;
	QVector<QPointF> m_points;
	QRectF m_bounds;
};

}

#endif // LASERTRAILITEM_H
