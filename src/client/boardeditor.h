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

class QColor;
class QPoint;

#include "core/brush.h"
#include "tools.h"

namespace network {
	class SessionState;
}

namespace interface {
	class BrushSource;
	class ColorSource;
}

namespace dpcore {
	class Point;
}

namespace drawingboard {

class Board;
class User;

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
		virtual ~BoardEditor() = 0;

		//! Check if the brush is currently in use
		virtual bool isCurrentBrush(const dpcore::Brush& brush) const = 0;

		//! Get the brush from the local UI
		dpcore::Brush localBrush() const;

		//! Set foreground color associated with the current board
		void setLocalForeground(const QColor& color);

		//! Set background color associated with the current board
		void setLocalBackground(const QColor& color);

		//! Get color from the board at the specified coordinates
		QColor colorAt(const QPoint& point);

		//! Start a preview
		void startPreview(tools::Type tool, const dpcore::Point& point, const dpcore::Brush& brush);

		//! Continue the preview
		void continuePreview(const dpcore::Point& point);

		//! Remove the preview
		void endPreview();

		//! Set the tool used for drawing
		virtual void setTool(const dpcore::Brush& brush) = 0;

		//! Add a new point to a stroke.
		virtual void addStroke(const dpcore::Point& point) = 0;

		//! End current stroke. Next addStroke will begin a new one.
		virtual void endStroke() = 0;

	protected:
		//! The local user
		User *user_;

		//! The board to access
		Board *board_;

	private:
		//! Source of the brush to use (UI)
		interface::BrushSource *brush_;

		//! Source of the color to use (UI)
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

		bool isCurrentBrush(const dpcore::Brush& brush) const;
		void setTool(const dpcore::Brush& brush);
		void addStroke(const dpcore::Point& point);
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

		bool isCurrentBrush(const dpcore::Brush& brush) const;
		void setTool(const dpcore::Brush& brush);
		void addStroke(const dpcore::Point& point);
		void endStroke();

	private:
		//! Remote session to which drawing commands are sent
		network::SessionState *session_;

		//! Last brush set
		/**
		 * The last brush is remembered to avoid sending redundant ToolInfo
		 * messages if the last message hasn't finished its round-trip yet.
		 */
		dpcore::Brush lastbrush_;
};

}

#endif

