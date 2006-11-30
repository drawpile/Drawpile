#include <iostream>
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

#include <QMouseEvent>
#include <QTabletEvent>
#include <QScrollBar>

#include "editorview.h"
#include "board.h"

namespace widgets {

EditorView::EditorView(QWidget *parent)
	: QGraphicsView(parent), pendown_(false), isdragging_(false),
	prevpoint_(0,0),outlinesize_(10), showoutline_(true), crosshair_(false)
{
}

void EditorView::setBoard(drawingboard::Board *board)
{
	board_ = board;
	setScene(board);
}

void EditorView::setOutline(bool enable)
{
	showoutline_ = enable;
	if(enable)
		viewport()->setMouseTracking(true);
	else
		viewport()->setMouseTracking(false);
}

void EditorView::setOutlineSize(int size)
{
	outlinesize_ = size;
}

void EditorView::setCrosshair(bool enable)
{
	crosshair_ = enable;
	if(enable)
		viewport()->setCursor(Qt::CrossCursor);
	else
		viewport()->setCursor(Qt::ArrowCursor);
}

void EditorView::enterEvent(QEvent *event)
{
	QGraphicsView::enterEvent(event);
	if(showoutline_)
		board_->showCursorOutline(prevpoint_,outlinesize_);
}

void EditorView::leaveEvent(QEvent *event)
{
	QGraphicsView::leaveEvent(event);
	board_->hideCursorOutline();
}

void EditorView::mousePressEvent(QMouseEvent *event)
{
	if(event->button() == Qt::MidButton) {
		startDrag(event->x(), event->y());
	} else {
		pendown_ = true;
		QPoint point = mapToScene(event->pos()).toPoint();
		emit penDown(point.x(), point.y(), 1.0, false);
	}
	board_->hideCursorOutline();

}

void EditorView::mouseMoveEvent(QMouseEvent *event)
{
	if(isdragging_) {
		moveDrag(event->x(), event->y());
	} else if(pendown_) {
		QPoint point = mapToScene(event->pos()).toPoint();
		emit penMove(point.x(), point.y(), 1.0);
	} else {
		QPoint point = mapToScene(event->pos()).toPoint();
		if(point != prevpoint_) {
			board_->moveCursorOutline(point);
			prevpoint_ = point;
		}
	}
}

void EditorView::mouseReleaseEvent(QMouseEvent *event)
{
	if(isdragging_) {
		stopDrag();
	} else {
		pendown_ = false;
		emit penUp();
	}
	if(showoutline_) {
		prevpoint_ = mapToScene(event->pos()).toPoint();
		board_->showCursorOutline(prevpoint_, outlinesize_);
	}
}

void EditorView::tabletEvent(QTabletEvent *event)
{
	event->accept();
	QPoint point = mapToScene(event->pos()).toPoint();
	if(event->pressure()==0) {
		if(pendown_) {
			pendown_ = false;
			if(showoutline_)
				board_->showCursorOutline(point,outlinesize_);
			emit penUp();
		} else {
			if(prevpoint_ != point) {
				board_->moveCursorOutline(point);
				prevpoint_ = point;
			}
		}
	} else {
		if(pendown_) {
			emit penMove(point.x(), point.y(), event->pressure());
		} else {
			emit penDown(point.x(), point.y(), event->pressure(),
					event->pointerType()==QTabletEvent::Eraser);
			pendown_ = true;
			board_->hideCursorOutline();
		}
	}
}

void EditorView::startDrag(int x,int y)
{
	oldcursor_ = cursor();
	setCursor(Qt::ClosedHandCursor);
	dragx_ = x;
	dragy_ = y;
	isdragging_ = true;
}

void EditorView::moveDrag(int x, int y)
{
	int dx = dragx_ - x;
	int dy = dragy_ - y;

	dragx_ = x;
	dragy_ = y;

	QScrollBar *ver = verticalScrollBar();
	ver->setSliderPosition(ver->sliderPosition()+dy);
	QScrollBar *hor = horizontalScrollBar();
	hor->setSliderPosition(hor->sliderPosition()+dx);
}

void EditorView::stopDrag()
{
	setCursor(oldcursor_);
	isdragging_ = false;
}

}

