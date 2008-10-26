/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2008 Calle Laakkonen

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
#include "boarditem.h"
#include "layerlistwidget.h"

namespace drawingboard {

User::User(BoardItem *board, int id)
	: id_(id), board_(board), layerlist_(0), layer_(0), strokestarted_(false)
{
}

void User::setLayerList(widgets::LayerList *ll)
{
	layerlist_ = ll;
	// Synchronize widget with current layer selection
	layerlist_->selectLayer(layer_);
}

void User::setLayerId(int id)
{
	layer_ = id;
	if(layerlist_)
		layerlist_->selectLayer(id);
}

/**
 * Starts or continues a stroke. If continuing, a line is drawn from the
 * previous coordinates to \a point.
 * @param point stroke coordinates
 */
void User::addStroke(const dpcore::Point& point)
{
	if(board_) {
		if(strokestarted_) {
			// Continuing stroke
			board_->drawLine(
					layer_,
					lastpoint_,
					point,
					brush_,
					strokelen_
					);
		} else {
			// First point
			board_->drawPoint(layer_, point, brush_);
			strokestarted_ = true;
			strokelen_ = 0;
		}
		lastpoint_ = point;
	}
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

