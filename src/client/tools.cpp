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
#include "toolsettingswidget.h"
#include "dualcolorbutton.h"
#include "board.h"

namespace tools {

Controller *Tool::controller_;
int Tool::user_;

/**
 * The returned tool can be used to perform actions on the board
 * controlled by the specified controller.
 * 
 * This is not multiboard safe! This class needs to be reworked
 * if the support for joining multiple boards simultaneously is needed.
 * @param controller board controller
 * @param user user id of the local user
 * @param type type of tool wanted
 * @return the requested tool
 */
Tool *Tool::get(Controller *controller, int user, Type type)
{
	// When and if we support joining to multiple boards,
	// tools can no longer be shared.
	static Tool *brush = new Brush();
	static Tool *eraser = new Eraser();
	static Tool *picker = new ColorPicker();
	controller_ = controller;
	user_ = user;
	switch(type) {
		case BRUSH: return brush;
		case ERASER: return eraser;
		case PICKER: return picker;
	}
	return 0;
}

void BrushBase::begin(int x,int y, qreal pressure)
{
	drawingboard::Brush brush = controller()->settings_->getBrush(
			controller()->colors_->foreground(),
			controller()->colors_->background()
			);
	controller()->board_->strokeBegin(user(),x,y,pressure,brush);
}

void BrushBase::motion(int x,int y, qreal pressure)
{
	controller()->board_->strokeMotion(user(),x,y,pressure);
}

void BrushBase::end()
{
	controller()->board_->strokeEnd(user());
}

void ColorPicker::begin(int x, int y, qreal pressure)
{
	(void)pressure;
	pickColor(x,y);
}

void ColorPicker::motion(int x,int y, qreal pressure)
{
	(void)pressure;
	pickColor(x,y);
}

void ColorPicker::end()
{
}

void ColorPicker::pickColor(int x,int y)
{
	QColor color = controller()->board_->colorAt(x,y);
	controller()->colors_->setForeground(color);
}

}

