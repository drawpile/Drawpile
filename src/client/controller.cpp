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
#include "tools.h"
#include "boardeditor.h"

Controller::Controller(QObject *parent)
	: QObject(parent), board_(0), editor_(0)
{
}

void Controller::setModel(drawingboard::Board *board,
		interface::BrushSource *brush,
		interface::ColorSource *color)
{
	board_ = board;
	board_->addUser(0);
	editor_ = board->getEditor(true);
	editor_->setBrushSource(brush);
	editor_->setColorSource(color);
}

void Controller::setTool(tools::Type tool)
{
	tool_ = tools::Tool::get(editor_,tool);
}

void Controller::penDown(const drawingboard::Point& point, bool isEraser)
{
	tool_->begin(point);
	if(tool_->readonly()==false)
		emit changed();
}

void Controller::penMove(const drawingboard::Point& point)
{
	tool_->motion(point);
}

void Controller::penUp()
{
	tool_->end();
}

