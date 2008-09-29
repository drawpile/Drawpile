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
#ifndef USER_H
#define USER_H

#include "core/point.h"
#include "core/brush.h"

namespace drawingboard {

class BoardItem;

//! A drawingboard user
/**
 * The user class holds user state information. It provides an interface
 * for committing drawing commands received from the network.
 */
class User
{
	public:
		User(int id);

		//! Get the user's ID number
		int id() const { return id_; }

		//! Set the layer on which to draw
		void setLayer(BoardItem *layer) { layer_ = layer; }

		//! Get the used layer
		BoardItem *layer() const { return layer_; }

		//! Set brush to use
		void setBrush(const dpcore::Brush& brush) { brush_ = brush; }

		//! Get the brush
		const dpcore::Brush& brush() const { return brush_; }

		//! Stroke info
		void addStroke(const dpcore::Point& point);

		//! End stroke
		void endStroke();

	private:
		int id_;
		dpcore::Brush brush_;

		BoardItem *layer_;
		dpcore::Point lastpoint_;
		bool strokestarted_;
		qreal strokelen_;
};

}

#endif


