/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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
#ifndef PPOINT_H
#define PPOINT_H

#include <cmath>
#include <QPoint>
#include <QVector>

namespace paintcore {

/**
 * @brief An extended point class that includes pressure information.
 */
class Point : public QPointF {
public:
	Point() : QPointF(), _p(1) {}

	Point(qreal x, qreal y, qreal p)
		: QPointF(x, y), _p(p)
	{
		Q_ASSERT(p>=0 && p<=1);
	}

	Point(const QPointF& point, qreal p)
		: QPointF(point), _p(p)
	{
		Q_ASSERT(p>=0 && p<=1);
	}

	Point(const QPoint& point, qreal p)
		: QPointF(point), _p(p)
	{
		Q_ASSERT(p>=0 && p<=1);
	}

	//! Get the pressure value for this point
	qreal pressure() const { return _p; }

	//! Get a reference to the pressure value of this point
	qreal &rpressure() { return _p; }

	//! Set this point's pressure value
	void setPressure(qreal p) { Q_ASSERT(p>=0 && p<=1); _p = p; }

	//! Compare two points at subpixel resolution
	bool roughlySame(const Point& point) const {
		qreal dx = x() - point.x();
		qreal dy = y() - point.y();
		qreal d = dx*dx + dy*dy;
		return d <= ((1/4.0)*(1/4.0));
	}

	//! Are the two points less than one pixel different?
	bool intSame(const Point &point) const {
		qreal dx = x() - point.x();
		qreal dy = y() - point.y();
		qreal d = dx*dx + dy*dy;
		return d < 1.0;
	}

	float distance(const QPointF &point) const
	{
		return hypot(point.x()-x(), point.y()-y());
	}

private:
	qreal _p;
};

static inline Point operator-(const Point& p1, const QPointF& p2)
{
	return Point(p1.x()-p2.x(), p1.y()-p2.y(), p1.pressure());
}

static inline Point operator+(const Point& p1, const QPointF& p2)
{
	return Point(p1.x()+p2.x(), p1.y()+p2.y(), p1.pressure());
}


typedef QVector<Point> PointVector;

}

#endif

