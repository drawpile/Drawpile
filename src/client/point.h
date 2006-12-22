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
#ifndef PPOINT_H
#define PPOINT_H

#include <QPoint>

namespace drawingboard {

//! An extended point class that includes pressure information.
class Point : public QPoint {
	public:
		Point() : QPoint(), p_(1) {}
		Point(int x, int y, qreal p)
			: QPoint(x,y), p_(p)
		{
			Q_ASSERT(p>=0 && p<=1);
		}

		Point(const QPoint& point, qreal p)
			: QPoint(point), p_(p)
		{
			Q_ASSERT(p>=0 && p<=1);
		}

		qreal pressure() const { return p_; }
		qreal &rpressure() { return p_; }
		void setPressure(qreal p) { p_ = p; }
	private:
		qreal p_;
};

static inline const Point operator-(const Point& p1,const QPoint& p2)
{
	return Point(p1.x()-p2.x(), p1.y()-p2.y(), p1.pressure());
}

}

#endif

