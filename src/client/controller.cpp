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

#include "controller.h"
#include "board.h"

Controller::Controller(QObject *parent)
	: QObject(parent), board_(0)
{
}

void Controller::setBoard(drawingboard::Board *board)
{
	board_ = board;
}

void Controller::penDown(int x,int y, qreal pressure, bool isEraser)
{
	if(board_) {
		board_->beginPreview(x,y,pressure);
	}
}

void Controller::penMove(int x,int y, qreal pressure)
{
	if(board_) {
		board_->strokePreview(x,y,pressure);
	}
}

void Controller::penUp()
{
	if(board_) {
		board_->endPreview();
	}
}

