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
#include <QPen>
#include "point.h"
#include "brush.h"
#include "preview.h"

namespace drawingboard {

//! Construct a stroke preview object
/**
 * @param parent parent layer
 * @param scene board to which this object belongs to
 */
Preview::Preview(QGraphicsItem *parent, QGraphicsScene *scene)
	: QGraphicsLineItem(parent, scene)
{
}

/**
 * @param from point from which the line begins
 * @param to point at which the line ends
 * @param brush brush to draw the line with
 */
void Preview::previewLine(const Point& from, const Point& to, const Brush& brush)
{
	brush_ = brush;
	from_ = from;
	to_ = to;
	QPen pen(brush.color(to.pressure()));
	int rad= brush.radius(to.pressure());
	if(rad==0) {
		pen.setWidth(1);
	} else {
		pen.setWidth(rad*2);
		pen.setCapStyle(Qt::RoundCap);
	}
	setPen(pen);

	setLine(from.x(), from.y(), to.x(), to.y());
	show();
}

}

