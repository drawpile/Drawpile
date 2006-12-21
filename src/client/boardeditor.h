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
#ifndef BOARDEDITOR_H
#define BOARDEDITOR_H

#include <QColor>

class QPoint;

namespace interface {
	class BrushSource;
	class ColorSource;
}

namespace drawingboard {

class Board;
class User;
class Brush;
class Point;

//! A delegate for accessing the board contents
/**
 * The board editor class provides an interface for local tools to
 * access the board directly and over the network.
 *
 * Read only methods always access the board directly, whereas those that
 * modify the board will either access directly or construct and dispatch
 * network packets.
 */
class BoardEditor {
	public:

		BoardEditor(Board *board, User *user) : board_(board), user_(user) {}
		virtual ~BoardEditor() {}

		//! Set the brush source
		void setBrushSource(interface::BrushSource *src);

		//! Set the color source
		void setColorSource(interface::ColorSource *src);

		//! Get the brush currently in use by the local user
		const Brush& currentBrush() const;

		//! Get the brush from the local UI
		Brush localBrush() const;

		//! Set foreground color associated with the current board
		void setLocalForeground(const QColor& color);

		//! Set background color associated with the current board
		void setLocalBackground(const QColor& color);

		//! Get color from the board at the specified coordinates
		QColor colorAt(const QPoint& point);

		//! Set the tool used for drawing
		virtual void setTool(const Brush& brush) = 0;

		//! Add a new point to a stroke.
		virtual void addStroke(const Point& point) = 0;

		//! End current stroke. Next addStroke will begin a new one.
		virtual void endStroke() = 0;

	protected:
		Board *board_;
		User *user_;
	private:
		interface::BrushSource *brush_;
		interface::ColorSource *color_;
};

//! Board editor that modifies the local board
class LocalBoardEditor : public BoardEditor {
	public:
		LocalBoardEditor(Board *board, User *user) : BoardEditor(board,user) {}

		void setTool(const Brush& brush);
		void addStroke(const Point& point);
		void endStroke();
};

}

#endif

