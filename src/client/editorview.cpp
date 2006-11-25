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
#include <QScrollBar>

#include "editorview.h"

namespace widgets {

EditorView::EditorView(QWidget *parent)
	: QGraphicsView(parent), pendown_(false), isdragging_(false)
{
	viewport()->setMouseTracking(false);
}

void EditorView::mousePressEvent(QMouseEvent *event)
{
	if(event->button() == Qt::MidButton) {
		startDrag(event->x(), event->y());
	} else {
		QPointF point = mapToScene(event->pos());
		emit penDown(qRound(point.x()), qRound(point.y()), 1.0, false);
	}
}

void EditorView::mouseMoveEvent(QMouseEvent *event)
{
	if(isdragging_) {
		moveDrag(event->x(), event->y());
	} else {
		QPointF point = mapToScene(event->pos());
		emit penMove(qRound(point.x()), qRound(point.y()), 1.0);
	}
}

void EditorView::mouseReleaseEvent(QMouseEvent *event)
{
	if(isdragging_) {
		stopDrag();
	} else {
		emit penUp();
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

