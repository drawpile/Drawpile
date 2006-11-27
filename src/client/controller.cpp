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
#include <iostream>
#include "controller.h"
#include "board.h"
#include "brush.h"
#include "dualcolorbutton.h"
#include "toolsettingswidget.h"

Controller::Controller(QObject *parent)
	: QObject(parent), board_(0)
{
}

void Controller::setBoard(drawingboard::Board *board)
{
	board_ = board;
	board_->addUser(0);
}

void Controller::penDown(int x,int y, qreal pressure, bool isEraser)
{
	//board_->beginPreview(x,y,pressure);
	drawingboard::Brush brush = settings_->getBrush(colors_->foreground(),
			colors_->background());
	board_->strokeBegin(0,x,y,pressure,brush);
}

void Controller::penMove(int x,int y, qreal pressure)
{
	//board_->strokePreview(x,y,pressure);
	board_->strokeMotion(0,x,y,pressure);
}

void Controller::penUp()
{
	//board_->endPreview();
	board_->strokeEnd(0);
}

