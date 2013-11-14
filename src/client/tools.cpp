/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2008 Calle Laakkonen

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

#include <QDebug>
#include "tools.h"
#include "toolsettings.h"
#include "core/brush.h"
#include "board.h"
#include "annotationitem.h"

namespace tools {

Tool::~Tool() { /* abstract */ }

/**
 * Construct one of each tool for the collection
 */
ToolCollection::ToolCollection()
	: editor_(0)
{
	tools_[PEN] = new Pen(*this);
	tools_[BRUSH] = new Brush(*this);
	tools_[ERASER] = new Eraser(*this);
	tools_[PICKER] = new ColorPicker(*this);
	tools_[LINE] = new Line(*this);
	tools_[RECTANGLE] = new Rectangle(*this);
	tools_[ANNOTATION] = new Annotation(*this);
}

/**
 * Delete the tools
 */
ToolCollection::~ToolCollection()
{
	foreach(Tool *t, tools_)
		delete t;
}

#if 0
/**
 * Set the board editor for this collection
 * @param editor BoardEditor to use
 */
void ToolCollection::setEditor(drawingboard::BoardEditor *editor)
{
	Q_ASSERT(editor);
	editor_ = editor;
}
#endif

void ToolCollection::setAnnotationSettings(AnnotationSettings *as)
{
	Q_ASSERT(as);
	as_ = as;
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
#if 0
	dpcore::Brush brush = editor()->localBrush();

	if(editor()->isCurrentBrush(brush) == false)
		editor()->setTool(brush);

	editor()->addStroke(point);
#endif
}

void BrushBase::motion(const dpcore::Point& point)
{
#if 0
	editor()->addStroke(point);
#endif
}

void BrushBase::end()
{
#if 0
	editor()->endStroke();
#endif
}

void ColorPicker::begin(const dpcore::Point& point)
{
#if 0
	QColor col = editor()->colorAt(point);
	if(col.isValid())
		editor()->setLocalForeground(col);
#endif
}

void ColorPicker::motion(const dpcore::Point& point)
{
#if 0
	QColor col = editor()->colorAt(point);
	if(col.isValid())
		editor()->setLocalForeground(col);
#endif
}

void ColorPicker::end()
{
}

void ComplexBase::begin(const dpcore::Point& point)
{
#if 0
	editor()->startPreview(type(), point, editor()->localBrush());
	start_ = point;
	end_ = point;
#endif
}

void ComplexBase::motion(const dpcore::Point& point)
{
#if 0
	editor()->continuePreview(point);
	end_ = point;
#endif
}

void ComplexBase::end()
{
#if 0
	editor()->endPreview();
	dpcore::Brush brush = editor()->localBrush();
	if(editor()->isCurrentBrush(brush) == false)
		editor()->setTool(brush);
	commit();
#endif
}

void Line::commit()
{
#if 0
	editor()->startAtomic();
	editor()->addStroke(start_);
	editor()->addStroke(end_);
	editor()->endStroke();
#endif
}

void Rectangle::commit()
{
#if 0
	using dpcore::Point;
	editor()->startAtomic();
	editor()->addStroke(start_);
	editor()->addStroke(Point(start_.x(), end_.y(), start_.pressure()));
	editor()->addStroke(end_);
	editor()->addStroke(Point(end_.x(), start_.y(), start_.pressure()));
	editor()->addStroke(start_ - Point(start_.x()<end_.x()?-1:1,0,1));
	editor()->endStroke();
#endif
}

/**
 * The annotation tool has fairly complex needs. Clicking on an existing
 * annotation selects it, otherwise a new annotation is started.
 */
void Annotation::begin(const dpcore::Point& point)
{
#if 0
	drawingboard::AnnotationItem *item = editor()->annotationAt(point);
	if(item) {
		sel_ = item;
		handle_ = sel_->handleAt(point - sel_->pos());
		aeditor()->setSelection(item);
	} else {
		editor()->startPreview(ANNOTATION, point, editor()->localBrush());
		end_ = point;
	}
	start_ = point;
#endif
}

/**
 * If we have a selected annotation, move or resize it. Otherwise extend
 * the preview rectangle for the new annotation.
 */
void Annotation::motion(const dpcore::Point& point)
{
#if 0
	if(sel_) {
		QPoint d = point - start_;
		switch(handle_) {
			case drawingboard::AnnotationItem::TRANSLATE:
				sel_->moveBy(d.x(), d.y());
				break;
			case drawingboard::AnnotationItem::RS_TOPLEFT:
				sel_->growTopLeft(d.x(), d.y());
				break;
			case drawingboard::AnnotationItem::RS_BOTTOMRIGHT:
				sel_->growBottomRight(d.x(), d.y());
				break;
		}
		start_ = point;
	} else {
		editor()->continuePreview(point);
		end_ = point;
	}
#endif
}

/**
 * If we have a selection, reannotate it. This is does nothing in local mode,
 * but is needed in a network session to inform the server of the new size/
 * position. (Room for optimization, don't reannotate if geometry didn't change)
 * If no existing annotation was selected, create a new one based on the
 * starting and ending coordinates. The new annotation is given some minimum
 * size to make sure something appears when the user just clicks and doesn't
 * move the mouse/stylus.
 */
void Annotation::end()
{
#if 0
	if(sel_) {
		// This is superfluous when in local mode, but needed
		// when in a networked session.
		// TODO
		protocol::Annotation a;
		sel_->getOptions(a);
		editor()->annotate(a);
		sel_ = 0;
	} else {
		editor()->endPreview();
		QRectF rect(QRectF(start_, end_));
		rect = rect.normalized();
		if(rect.width()<15)
			rect.setWidth(15);
		if(rect.height()<15)
			rect.setHeight(15);
		protocol::Annotation a;
		a.rect = rect.toRect();
		a.textcolor = editor()->localBrush().color(1.0).name();
		a.backgroundcolor = editor()->localBrush().color(0.0).name();
		editor()->annotate(a);
	}
#endif
}

}

