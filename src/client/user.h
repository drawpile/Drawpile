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

#include "brush.h"

namespace drawingboard {

class Layer;

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

		//! Get a reference to the user's brush
		Brush& brush() {return brush_;}

		//! Get the user's brush
		const Brush& brush() const {return brush_;}

		//! Begin a new stroke
		void strokeBegin(Layer *layer, int x, int y, qreal pressure);

		//! Continue stroke
		void strokeMotion(int x,int y, qreal pressure);

		//! End stroke
		void strokeEnd();
	private:
		int id_;
		Brush brush_;

		Layer *layer_;
		int lastx_,lasty_;
		qreal lastpressure_;
		bool penmoved_;
};

}

#endif


