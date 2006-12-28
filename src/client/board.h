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
#include <QHash>

//! Drawing board related classes
namespace drawingboard {

class Layer;
class User;
class Brush;
class BoardEditor;
class Point;

//! The drawing board
/**
 * The drawing board contains the picture and provides methods
 * to modify it. The board has a list of users and remembers each of their
 * states, so drawing commands can be interleaved.
 */
class Board : public QGraphicsScene
{
	Q_OBJECT
	friend class BoardEditor;

	public:
		Board(QObject *parent=0);
		~Board();

		//! Initialize to a solid color
		void initBoard(const QSize& size, const QColor& background);

		//! Initialize the board using an existing image as base
		void initBoard(QImage image);

		//! Get board contents as an image
		QImage image() const;

		//! Add a new user to the board
		void addUser(int id);

		//! Remove a user from the board
		void removeUser(int id);

		//! Get a board editor
		BoardEditor *getEditor(bool local);

#if 0 // move these to BoardEditor?
		//! Begin a new preview stroke
		void previewBegin(int x,int y, qreal pressure);

		//! Pen motion info for preview stroke
		void previewMotion(int x,int y, qreal pressure);

		//! End a preview stroke
		void previewEnd();
#endif

		//! User switches tools
		void userSetTool(int user, const Brush& brush);

		//! User stroke information
		void userStroke(int user, const Point& point);

		//! User ends a stroke
		void userEndStroke(int user);

	private:
		Layer *image_;
		QHash<int,User*> users_;
};

}

#endif


