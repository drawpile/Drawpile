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
	prevpoint_(0,0),outlinesize_(10), enableoutline_(true), showoutline_(true), crosshair_(false)
{
}

void EditorView::setBoard(drawingboard::Board *board)
{
	board_ = board;
	setScene(board);
}

void EditorView::setOutline(bool enable)
{
	enableoutline_ = enable;
	if(enable)
		viewport()->setMouseTracking(true);
	else
		viewport()->setMouseTracking(false);
}

void EditorView::setOutlineRadius(int radius)
{
	outlinesize_ = radius;
}

void EditorView::setCrosshair(bool enable)
{
	crosshair_ = enable;
	if(enable)
		viewport()->setCursor(Qt::CrossCursor);
	else
		viewport()->setCursor(Qt::ArrowCursor);
}

void EditorView::drawForeground(QPainter *painter, const QRectF& rect)
{
	if(enableoutline_ && showoutline_) {
		painter->drawEllipse(
				QRectF(prevpoint_-QPointF(outlinesize_,outlinesize_),
				QSizeF(outlinesize_*2,outlinesize_*2))
				);
	}
}

void EditorView::enterEvent(QEvent *event)
{
	QGraphicsView::enterEvent(event);
	showoutline_ = true;
}

void EditorView::leaveEvent(QEvent *event)
{
	QGraphicsView::leaveEvent(event);
	if(enableoutline_) {
		showoutline_ = false;
		QList<QRectF> rect;
		rect.append(QRectF(prevpoint_.x() - outlinesize_,
					prevpoint_.y() - outlinesize_,
					outlinesize_*2, outlinesize_*2));
		updateScene(rect);
	}
}

void EditorView::mousePressEvent(QMouseEvent *event)
{
	if(event->button() == Qt::MidButton) {
		startDrag(event->x(), event->y());
		if(enableoutline_) {
			showoutline_ = false;
			QList<QRectF> rect;
			rect.append(QRectF(prevpoint_.x() - outlinesize_,
						prevpoint_.y() - outlinesize_,
						outlinesize_*2, outlinesize_*2));
			updateScene(rect);
		}
	} else {
		pendown_ = true;
		QPoint point = mapToScene(event->pos()).toPoint();
		emit penDown(point.x(), point.y(), 1.0, false);
	}
}

void EditorView::mouseMoveEvent(QMouseEvent *event)
{
	if(isdragging_) {
		moveDrag(event->x(), event->y());
	} else {
		QPoint point = mapToScene(event->pos()).toPoint();
		if(point != prevpoint_) {
			if(pendown_)
				emit penMove(point.x(), point.y(), 1.0);
			else if(enableoutline_ && showoutline_) {
				QList<QRectF> rect;
				const int dia = outlinesize_*2;
				rect.append(QRectF(prevpoint_.x() - outlinesize_,
							prevpoint_.y() - outlinesize_, dia,dia));
				rect.append(QRectF(point.x() - outlinesize_,
							point.y() - outlinesize_, dia,dia));
				updateScene(rect);
			}
			prevpoint_ = point;
		}
	}
}

void EditorView::mouseReleaseEvent(QMouseEvent *event)
{
	prevpoint_ = mapToScene(event->pos()).toPoint();
	if(isdragging_) {
		stopDrag();
		showoutline_ = true;
	} else {
		pendown_ = false;
		emit penUp();
	}
}

void EditorView::mouseDoubleClickEvent(QMouseEvent*)
{
	// Ignore doubleclicks
}

void EditorView::tabletEvent(QTabletEvent *event)
{
	event->accept();
	QPoint point = mapToScene(event->pos()).toPoint();

#if 0
	if(prevpoint_ != point)
		board_->moveCursorOutline(point);
#endif

	if(event->pressure()==0) {
		// Pressure 0, pen is not touching the tablet
		if(pendown_) {
			pendown_ = false;
			emit penUp();
		}
	} else {
		// Pressure>0, pen is touching the tablet
		if(pendown_) {
			if(prevpoint_ != point)
				emit penMove(point.x(), point.y(), event->pressure());
		} else {
			emit penDown(point.x(), point.y(), event->pressure(),
					event->pointerType()==QTabletEvent::Eraser);
			pendown_ = true;
			prevpoint_ = point;
		}
	}
	prevpoint_ = point;
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

