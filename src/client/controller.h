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
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>

#include "tools.h"

namespace drawingboard {
	class Board;
	class BoardEditor;
}

//! Controller for drawing and network operations
/**
 * The controller handles all drawing commands coming in from the
 * network or the user. Drawing commands received from the network
 * are committed to the board and user commands are sent to the server.
 *
 * Before finishing their roundtrip from the server, user commands
 * are displayed on a special preview layer. This provides immediate
 * feedback even when the network is congested. Preview strokes are
 * removed as the real ones are received from the server.
 */
class Controller : public QObject
{
	Q_OBJECT
	friend class tools::BrushBase;
	friend class tools::ColorPicker;
	public:
		Controller(QObject *parent=0);

		void setBoard(drawingboard::Board *board);

	public slots:
		void penDown(int x,int y, qreal pressure, bool isEraser);
		void penMove(int x,int y, qreal pressure);
		void penUp();
		void setTool(tools::Type tool);

	private:
		drawingboard::Board *board_;
		tools::Tool *tool_;
		drawingboard::BoardEditor *editor_;
};

#endif

