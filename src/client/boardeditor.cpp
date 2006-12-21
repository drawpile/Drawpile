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

#include "boardeditor.h"
#include "layer.h"
#include "board.h"
#include "user.h"
#include "interfaces.h"

namespace drawingboard {

void BoardEditor::setBrushSource(interface::BrushSource *src)
{
	brush_ = src;
}

void BoardEditor::setColorSource(interface::ColorSource *src)
{
	color_ = src;
}

const Brush& BoardEditor::currentBrush() const
{
	return user_->brush();
}

Brush BoardEditor::localBrush() const
{
	return brush_->getBrush();
}

void BoardEditor::setLocalForeground(const QColor& color)
{
	color_->setForeground(color);
}

void BoardEditor::setLocalBackground(const QColor& color)
{
	color_->setBackground(color);
}

QColor BoardEditor::colorAt(const QPoint& point)
{
	return board_->image_->image().pixel(point);
}

void LocalBoardEditor::setTool(const Brush& brush)
{
	user_->setBrush(brush);
}

void LocalBoardEditor::addStroke(const Point& point)
{
	user_->addStroke(point);
}

void LocalBoardEditor::endStroke()
{
	user_->endStroke();
}

}

