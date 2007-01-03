
/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

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
#include "point.h"
#include "preview.h"

namespace drawingboard {

//! Construct a stroke preview object
/**
 * @param prev if not null, continue a previous stroke
 * @param point stroke end point
 * @param parent parent layer
 * @param scene board to which this object belongs to
 */
Preview::Preview(const Point *prev, const Point& point,
		QGraphicsItem *parent, QGraphicsScene *scene)
	: QGraphicsLineItem(parent, scene)
{
	if(prev) {
		setLine(prev->x(), prev->y(), point.x(), point.y());
	} else {
		setLine(point.x(), point.y(), point.x(), point.y());
	}
}

}

