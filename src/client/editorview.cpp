#include <QDebug>
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
#include "point.h"

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

/**
 * @param enable if true, brush outline is shown
 */
void EditorView::setOutline(bool enable)
{
	enableoutline_ = enable;
	if(enable)
		viewport()->setMouseTracking(true);
	else
		viewport()->setMouseTracking(false);
}

/**
 * A solid circle is first drawn with the background color,
 * then a dottet circle is drawn over it using the foreground color.
 * @param fg foreground color for outline
 * @param bg background color for outline
 */
void EditorView::setOutlineColors(const QColor& fg, const QColor& bg)
{
	foreground_ = fg;
	background_ = bg;
	if(enableoutline_ && showoutline_) {
		QList<QRectF> rect;
		rect.append(QRectF(prevpoint_.x() - outlinesize_,
					prevpoint_.y() - outlinesize_,
					outlinesize_*2, outlinesize_*2));
		updateScene(rect);
	}
}

/**
 * @param radius circle radius
 */
void EditorView::setOutlineRadius(int radius)
{
	int updatesize = outlinesize_;
	outlinesize_ = radius;
	if(enableoutline_ && showoutline_) {
		if(outlinesize_>updatesize)
			updatesize = outlinesize_;
		QList<QRectF> rect;
		rect.append(QRectF(prevpoint_.x() - updatesize,
					prevpoint_.y() - updatesize,
					updatesize*2, updatesize*2));
		updateScene(rect);
	}
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
	if(enableoutline_ && showoutline_ && outlinesize_>0) {
		QRectF rect(prevpoint_-QPointF(outlinesize_,outlinesize_),
					QSizeF(outlinesize_*2,outlinesize_*2));
		painter->setRenderHint(QPainter::Antialiasing, true);
		QPen pen(background_);
		painter->setPen(pen);
		painter->drawEllipse(rect);
		pen.setColor(foreground_);
		pen.setStyle(Qt::DashLine);
		painter->setPen(pen);
		painter->drawEllipse(rect);

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
		emit penDown(
				drawingboard::Point(mapToScene(event->pos()).toPoint(), 1.0),
				false
				);
	}
}

void EditorView::mouseMoveEvent(QMouseEvent *event)
{
	if(pendown_ && event->buttons() == Qt::NoButton) {
		// In case we missed a penup
		mouseReleaseEvent(event);
		return;
	}

	if(isdragging_) {
		moveDrag(event->x(), event->y());
	} else {
		QPoint point = mapToScene(event->pos()).toPoint();
		if(point != prevpoint_) {
			if(pendown_)
				emit penMove(drawingboard::Point(point, 1.0));
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

bool EditorView::viewportEvent(QEvent *event)
{
	if(event->type() == QEvent::TabletMove) {
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		tabev->accept();
		QPoint point = mapToScene(tabev->pos()).toPoint();
		if(pendown_) {
			emit penDown(
					drawingboard::Point(point.x(),point.y(),tabev->pressure()),
					tabev->pointerType()==QTabletEvent::Eraser
					);
		} else if(enableoutline_ && showoutline_) {
			QList<QRectF> rect;
			const int dia = outlinesize_*2;
			rect.append(QRectF(prevpoint_.x() - outlinesize_,
						prevpoint_.y() - outlinesize_, dia,dia));
			rect.append(QRectF(point.x() - outlinesize_,
						point.y() - outlinesize_, dia,dia));
			updateScene(rect);
		}
		prevpoint_ = point;
		return true;
	} else if(event->type() == QEvent::TabletPress) {
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		tabev->accept();
		QPoint point = mapToScene(tabev->pos()).toPoint();

		pendown_ = true;
		emit penDown(
				drawingboard::Point(mapToScene(tabev->pos()).toPoint(),
					tabev->pressure()),
				false
				);
		prevpoint_ = point;
		return true;
	} else if(event->type() == QEvent::TabletRelease) {
		pendown_ = false;
		emit penUp();
		return true;
	}
	return QAbstractScrollArea::viewportEvent(event);
}

//! Start dragging the view
void EditorView::startDrag(int x,int y)
{
	viewport()->setCursor(Qt::ClosedHandCursor);
	dragx_ = x;
	dragy_ = y;
	isdragging_ = true;
}

//! Drag the view
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

//! Stop dragging
void EditorView::stopDrag()
{
	setCrosshair(crosshair_);
	isdragging_ = false;
}

}

