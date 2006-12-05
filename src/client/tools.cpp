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
#include "tools.h"
#include "brush.h"
#include "board.h"
#include "boardeditor.h"
#include "interfaces.h"

namespace tools {

drawingboard::BoardEditor *Tool::editor_;

/**
 * The returned tool can be used to perform actions on the board
 * controlled by the specified controller.
 * 
 * This is not multiboard safe! This class needs to be reworked
 * if the support for joining multiple boards simultaneously is needed.
 * @param editor board editor delegate
 * @param type type of tool wanted
 * @return the requested tool
 */
Tool *Tool::get(drawingboard::BoardEditor *editor, Type type)
{
	// When and if we support joining to multiple boards,
	// tools can no longer be shared.
	static Tool *brush = new Brush();
	static Tool *eraser = new Eraser();
	static Tool *picker = new ColorPicker();
	editor_ = editor;
	switch(type) {
		case BRUSH: return brush;
		case ERASER: return eraser;
		case PICKER: return picker;
	}
	return 0;
}

void BrushBase::begin(int x,int y, qreal pressure)
{
	drawingboard::Brush brush = interface::Global::brushSource()->getBrush(
			interface::Global::colorSource()->foreground(),
			interface::Global::colorSource()->background()
			);

	if(editor_->currentBrush() != brush)
		editor_->setTool(brush);

	editor_->addStroke(x,y,pressure);
}

void BrushBase::motion(int x,int y, qreal pressure)
{
	editor_->addStroke(x,y,pressure);
}

void BrushBase::end()
{
	editor_->endStroke();
}

void ColorPicker::begin(int x, int y, qreal pressure)
{
	(void)pressure;
	interface::Global::colorSource()->setForeground(
			editor_->colorAt(x,y));
}

void ColorPicker::motion(int x,int y, qreal pressure)
{
	(void)pressure;
	interface::Global::colorSource()->setForeground(
			editor_->colorAt(x,y));
}

void ColorPicker::end()
{
}

}

