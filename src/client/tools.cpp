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
#include "core/brush.h"
#include "board.h"
#include "boardeditor.h"

namespace tools {

Tool::~Tool() { /* abstract */ }

/**
 * Construct one of each tool for the collection
 */
ToolCollection::ToolCollection()
	: editor_(0)
{
	tools_[BRUSH] = new Brush(*this);
	tools_[ERASER] = new Eraser(*this);
	tools_[PICKER] = new ColorPicker(*this);
	tools_[LINE] = new Line(*this);
	tools_[RECTANGLE] = new Rectangle(*this);
}

/**
 * Delete the tools
 */
ToolCollection::~ToolCollection()
{
	foreach(Tool *t, tools_)
		delete t;
}

/**
 * Set the board editor for this collection
 * @param editor BoardEditor to use
 */
void ToolCollection::setEditor(drawingboard::BoardEditor *editor)
{
	Q_ASSERT(editor);
	editor_ = editor;
}

/**
 * The returned tool can be used to perform actions on the board
 * controlled by the specified controller.
 * 
 * @param type type of tool wanted
 * @return the requested tool
 */
Tool *ToolCollection::get(Type type)
{
	Q_ASSERT(tools_.contains(type));
	return tools_.value(type);
}

void BrushBase::begin(const dpcore::Point& point)
{
	dpcore::Brush brush = editor()->localBrush();

	if(editor()->isCurrentBrush(brush) == false)
		editor()->setTool(brush);

	editor()->addStroke(point);
}

void BrushBase::motion(const dpcore::Point& point)
{
	editor()->addStroke(point);
}

void BrushBase::end()
{
	editor()->endStroke();
}

void ColorPicker::begin(const dpcore::Point& point)
{
	QColor col = editor()->colorAt(point);
	if(col.isValid())
		editor()->setLocalForeground(col);
}

void ColorPicker::motion(const dpcore::Point& point)
{
	QColor col = editor()->colorAt(point);
	if(col.isValid())
		editor()->setLocalForeground(col);
}

void ColorPicker::end()
{
}

void ComplexBase::begin(const dpcore::Point& point)
{
	editor()->startPreview(type(), point, editor()->localBrush());
	start_ = point;
	end_ = point;
}

void ComplexBase::motion(const dpcore::Point& point)
{
	editor()->continuePreview(point);
	end_ = point;
}

void ComplexBase::end()
{
	editor()->endPreview();
	dpcore::Brush brush = editor()->localBrush();
	if(editor()->isCurrentBrush(brush) == false)
		editor()->setTool(brush);
	commit();
}

void Line::commit()
{
	editor()->addStroke(start_);
	editor()->addStroke(end_);
	editor()->endStroke();
}

void Rectangle::commit()
{
	using dpcore::Point;
	editor()->addStroke(start_);
	editor()->addStroke(Point(start_.x(), end_.y(), start_.pressure()));
	editor()->addStroke(end_);
	editor()->addStroke(Point(end_.x(), start_.y(), start_.pressure()));
	editor()->addStroke(start_ - Point(start_.x()<end_.x()?-1:1,0,1));
	editor()->endStroke();
}

}

