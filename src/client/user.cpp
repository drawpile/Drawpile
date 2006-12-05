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

#include "user.h"
#include "layer.h"

namespace drawingboard {

User::User(int id)
	: id_(id), strokestarted_(false)
{
}

void User::setLayer(Layer *layer)
{
	layer_ = layer;
}

void User::setBrush(const Brush& brush)
{
	brush_ = brush;
}

/**
  @param x x coordinate
  @param y y coordinate
  @param pressure pressure [0..1]
 */
void User::addStroke(int x,int y, qreal pressure)
{
	QPoint point(x,y);
	if(strokestarted_) {
		// Continuing stroke
		layer_->drawLine(
				lastpoint_, lastpressure_,
				point, pressure,
				brush_
				);
	} else {
		// First point
		layer_->drawPoint(point, pressure, brush_);
		strokestarted_ = true;
	}
	lastpoint_ = point;
	lastpressure_ = pressure;
}

/**
 * endStroke resets the pen so the next addStroke will begin a new stroke.
 * If the pen did not move during between the first addStroke() and endStroke()
 * a single dot will be drawn.
 */
void User::endStroke()
{
	strokestarted_ = false;
}

}

