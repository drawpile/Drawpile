/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

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

#include <cmath>

#include <QPoint>
#include <QVector>

namespace dpcore {

//! An extended point class that includes pressure information.
/**
 * The regular x() and y() methods return coordinates relative to the top
 * left corner of the pixel. For subpixel precision, use
 * cx() + xFrac() and cy() + yFrac() to get corrected coordinates.
 */
class Point : public QPoint {
	public:
		Point() : QPoint(), p_(1), xfrac_(0.5), yfrac_(0.5) {}

		//! Construct a point with integer coordinates
		Point(int x, int y, qreal p)
			: QPoint(x,y), p_(p), xfrac_(0.5), yfrac_(0.5)
		{
			Q_ASSERT(p>=0 && p<=1);
		}

		//! Construct a point with subpixel accuracy
		Point(const QPointF& point, qreal p)
			: QPoint(int(point.x()), int(point.y())), p_(p)
		{
			Q_ASSERT(p>=0 && p<=1);
			double tmp;
			xfrac_ = fabs(modf(point.x(), &tmp));
			yfrac_ = fabs(modf(point.y(), &tmp));
		}

		//! Construct a point from a QPoint + pressure value
		Point(const QPoint& point, qreal p)
			: QPoint(point), p_(p), xfrac_(0.5), yfrac_(0.5)
		{
			Q_ASSERT(p>=0 && p<=1);
		}

		//! Get the pressure value for this point
		qreal pressure() const { return p_; }

		//! Get a reference to the pressure value of this point
		qreal &rpressure() { return p_; }

		//! Set this point's pressure value
		void setPressure(qreal p) { p_ = p; }

		//! Get the horizontal subpixel offset
		qreal xFrac() const { return xfrac_; }

		//! Get the vertical subpixel offset
		qreal yFrac() const { return yfrac_; }

		//! Compare two points just by their integer coordinates
		bool intSame(const Point& point) const {
			return x() == point.x() && y()==point.y();
		}

		//! Compare two points at subpixel resolution
		bool roughlySame(const Point& point) const {
			return intSame(point) && fabs(xfrac_-point.xfrac_)<(1/4.0) &&
				fabs(yfrac_-point.yfrac_)<(1/4.0);
		}

	private:
		qreal p_;
		qreal xfrac_, yfrac_;
};

static inline const Point operator-(const Point& p1,const QPoint& p2)
{
	return Point(p1.x()-p2.x(), p1.y()-p2.y(), p1.pressure());
}

typedef QVector<Point> PointVector;

}

#endif

