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
#ifndef BOARD_H
#define BOARD_H

#include <QGraphicsScene>

#include "brush.h" // for testing

//! Drawing board related classes
namespace drawingboard {

class Layer;

//! The drawing board
/**
 */
class Board : public QGraphicsScene
{
	Q_OBJECT
	public:
		Board(QObject *parent=0);

		//! Initialize to a solid color
		void initBoard(const QSize& size, const QColor& background);

		//! Initialize the board using an existing pixmap as base
		void initBoard(QPixmap pixmap);

		//! Begin a new preview stroke
		void beginPreview(int x,int y, qreal pressure);
		//! Pen motion info for preview stroke
		void strokePreview(int x,int y, qreal pressure);
		//! End a preview stroke
		void endPreview();

	private:
		Layer *image_;

		Brush brush_;
		int plastx_, plasty_;
};

}

#endif


