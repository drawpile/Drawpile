
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
#ifndef PREVIEW_H
#define PREVIEW_H

#include <QGraphicsLineItem>

namespace drawingboard {

//! Stroke feedback
/**
 * The stroke feedback object provides immediate feedback for the user
 * while the drawing commands are making their roundtrip through the server.
 * It is not used in offline mode, where drawing commands are committed
 * directly to the board.
 */
class Preview : public QGraphicsLineItem {
	public:
		Preview(Preview *last, const Point& point,
				QGraphicsItem *parent, QGraphicsScene *scene);
};

}

#endif

