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

#include "controller.h"
#include "tools.h"
#include "brush.h"
#include "board.h"
#include "boardeditor.h"

namespace tools {

drawingboard::BoardEditor *Tool::editor_;

void Tool::setEditor(drawingboard::BoardEditor *editor)
{
	Q_ASSERT(editor);
	editor_ = editor;
}

/**
 * The returned tool can be used to perform actions on the board
 * controlled by the specified controller.
 * 
 * This is not multiboard safe! This class needs to be reworked
 * if support for joining multiple boards simultaneously is needed.
 * @param type type of tool wanted
 * @return the requested tool
 */
Tool *Tool::get(Type type)
{
	// When and if we support joining to multiple boards,
	// tools can no longer be shared.
	static Tool *brush = new Brush();
	static Tool *eraser = new Eraser();
	static Tool *picker = new ColorPicker();
	static Line *line = new Line();
	static Rectangle *rect = new Rectangle();
	switch(type) {
		case BRUSH: return brush;
		case ERASER: return eraser;
		case PICKER: return picker;
		case LINE: return line;
		case RECTANGLE: return rect;
	}
	return 0;
}

void BrushBase::begin(const drawingboard::Point& point)
{
	drawingboard::Brush brush = editor_->localBrush();

	if(editor_->isCurrentBrush(brush) == false)
		editor_->setTool(brush);

	editor_->addStroke(point);
}

void BrushBase::motion(const drawingboard::Point& point)
{
	editor_->addStroke(point);
}

void BrushBase::end()
{
	editor_->endStroke();
}

void ColorPicker::begin(const drawingboard::Point& point)
{
	QColor col = editor_->colorAt(point);
	if(col.isValid())
		editor_->setLocalForeground(col);
}

void ColorPicker::motion(const drawingboard::Point& point)
{
	QColor col = editor_->colorAt(point);
	if(col.isValid())
		editor_->setLocalForeground(col);
}

void ColorPicker::end()
{
}

void ComplexBase::begin(const drawingboard::Point& point)
{
	editor_->startPreview(type(), point, editor_->localBrush());
	start_ = point;
	end_ = point;
}

void ComplexBase::motion(const drawingboard::Point& point)
{
	editor_->continuePreview(point);
	end_ = point;
}

void ComplexBase::end()
{
	editor_->endPreview();
	drawingboard::Brush brush = editor_->localBrush();
	if(editor_->isCurrentBrush(brush) == false)
		editor_->setTool(brush);
	commit();
}

void Line::commit()
{
	editor_->addStroke(start_);
	editor_->addStroke(end_);
	editor_->endStroke();
}

void Rectangle::commit()
{
	using drawingboard::Point;
	editor_->addStroke(start_);
	editor_->addStroke(Point(start_.x(), end_.y(), start_.pressure()));
	editor_->addStroke(end_);
	editor_->addStroke(Point(end_.x(), start_.y(), start_.pressure()));
	editor_->addStroke(start_ - Point(start_.x()<end_.x()?-1:1,0,1));
	editor_->endStroke();
}

}

