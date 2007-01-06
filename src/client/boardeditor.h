/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2007 Calle Laakkonen

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

namespace network {
	class SessionState;
}

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
 * Read only methods always access the board directly whereas those that
 * modify the board will either access directly or construct and dispatch
 * network packets.
 */
class BoardEditor {
	public:

		//! Construct a board editor
		BoardEditor(Board *board, User *user, interface::BrushSource *brush,
				interface::ColorSource *color);
		virtual ~BoardEditor() {}

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

		//! Resend tool info. Does nothing on LocalBoardEditor
		virtual void resendBrush() {}

	protected:
		User *user_;
		Board *board_;
	private:
		interface::BrushSource *brush_;
		interface::ColorSource *color_;
};

//! Board editor that modifies the local board
/**
 * The commands are relayed to the local user object.
 */
class LocalBoardEditor : public BoardEditor {
	public:
		//! Construct a local board editor
		LocalBoardEditor(Board *board, User *user,
				interface::BrushSource *brush, interface::ColorSource *color)
			: BoardEditor(board,user, brush, color) {}

		void setTool(const Brush& brush);
		void addStroke(const Point& point);
		void endStroke();
};

//! Board editor that modifies the board through the network
/**
 * The editing commands are sent to the hosting server.
 */
class RemoteBoardEditor : public BoardEditor {
	public:
		//! Construct a remote board editor
		RemoteBoardEditor(Board *board, User *user,
				network::SessionState *session, interface::BrushSource *brush,
				interface::ColorSource *color);

		void setTool(const Brush& brush);
		void addStroke(const Point& point);
		void endStroke();
		void resendBrush();
	private:
		network::SessionState *session_;
};

}

#endif

