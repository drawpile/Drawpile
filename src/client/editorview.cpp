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

#include <QMouseEvent>
#include <QTabletEvent>
#include <QScrollBar>
#include <QUrl>
#include <QBitmap>
#include <QPainter>

#include "editorview.h"
#include "board.h"
#include "core/point.h"

namespace widgets {

EditorView::EditorView(QWidget *parent)
	: QGraphicsView(parent), pendown_(NOTDOWN), isdragging_(NOTRANSFORM),
	dragbtndown_(NOTRANSFORM), outlinesize_(10), dia_(20),
	enableoutline_(true), showoutline_(true)
{
	viewport()->setAcceptDrops(true);
	setAcceptDrops(true);

	// Draw the crosshair cursor
	QBitmap bm(32,32);
	bm.clear();
	QPainter bmp(&bm);
	bmp.setPen(Qt::color1);
	bmp.drawLine(16,0,16,32);
	bmp.drawLine(0,16,32,16);
	QBitmap mask = bm;
	bmp.setPen(Qt::color0);
	bmp.drawPoint(16,16);
	cursor_ = QCursor(bm, mask);
	viewport()->setCursor(cursor_);
}

void EditorView::setBoard(drawingboard::Board *board)
{
	board_ = board;
	setScene(board);
	// notify of scene change
	sceneChanged();
}

/**
 * @param enable if true, brush outline is shown
 */
void EditorView::setOutline(bool enable)
{
	enableoutline_ = enable;
	viewport()->setMouseTracking(enable);
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
					dia_, dia_));
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
	dia_ = radius*2;
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

void EditorView::drawForeground(QPainter *painter, const QRectF& rect)
{
	if(enableoutline_ && showoutline_ && outlinesize_>1) {
		const QRectF outline(prevpoint_-QPointF(outlinesize_,outlinesize_),
					QSizeF(dia_, dia_));
		if(rect.intersects(outline)) {
			//painter->setClipRect(0,0,board_->width(),board_->height()); // default
			//painter->setClipRect(outline.adjusted(-7, -7, 7, 7)); // smaller clipping
			//painter->setRenderHint(QPainter::Antialiasing, true); // default
			QPen pen(background_);
			painter->setPen(pen);
			painter->drawEllipse(outline);
			pen.setColor(foreground_);
			pen.setStyle(Qt::DashLine);
			painter->setPen(pen);
			painter->drawEllipse(outline);
		}
	}
}

void EditorView::enterEvent(QEvent *event)
{
	QGraphicsView::enterEvent(event);
	if(enableoutline_) {
		showoutline_ = true;
	}
	// Give focus to this widget on mouseover. This is so that
	// using spacebar for dragging works rightaway.
	setFocus(Qt::MouseFocusReason);
}

void EditorView::leaveEvent(QEvent *event)
{
	QGraphicsView::leaveEvent(event);
	if(enableoutline_) {
		showoutline_ = false;
		QList<QRectF> rect;
		rect.append(QRectF(prevpoint_.x() - outlinesize_,
					prevpoint_.y() - outlinesize_,
					dia_, dia_));
		updateScene(rect);
	}
}

//! Handle mouse press events
void EditorView::mousePressEvent(QMouseEvent *event)
{
	/** @todo why do we sometimes get mouse events for tablet strokes? */
	if(pendown_ != NOTDOWN)
		return;
	if(event->button() == Qt::MidButton || dragbtndown_) {
		startDrag(event->x(), event->y(), dragbtndown_!=ROTATE?TRANSLATE:ROTATE);
	} else if(event->button() == Qt::LeftButton && isdragging_==NOTRANSFORM) {
		pendown_ = MOUSEDOWN;
		emit penDown(
				dpcore::Point(mapToScene(event->pos()), 1.0)
				);
	}
}

//! Handle mouse motion events
void EditorView::mouseMoveEvent(QMouseEvent *event)
{
	/** @todo why do we sometimes get mouse events for tablet strokes? */
	if(pendown_ == TABLETDOWN)
		return;
	if(pendown_ && event->buttons() == Qt::NoButton) {
		// In case we missed a mouse release
		mouseReleaseEvent(event);
		return;
	}

	if(isdragging_) {
		moveDrag(event->x(), event->y());
	} else {;
		const dpcore::Point point(mapToScene(event->pos()), 1.0);
		if(!prevpoint_.intSame(point)) {
			if(pendown_)
				emit penMove(point);
			else
				updateOutline(point);
			prevpoint_ = point;
		}
	}
}

//! Handle mouse release events
void EditorView::mouseReleaseEvent(QMouseEvent *event)
{
	if(pendown_ == TABLETDOWN)
		return;
	prevpoint_ = dpcore::Point(mapToScene(event->pos()), 0.0);
	if(isdragging_) {
		stopDrag();
	} else if(event->button() == Qt::LeftButton && pendown_ == MOUSEDOWN) {
		pendown_ = NOTDOWN;
		emit penUp();
	}
}

void EditorView::mouseDoubleClickEvent(QMouseEvent*)
{
	// Ignore doubleclicks
}

void EditorView::keyPressEvent(QKeyEvent *event) {
	if(event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
		event->accept();
		if(event->modifiers() & Qt::ControlModifier) {
			dragbtndown_ = ROTATE;
		} else {
			dragbtndown_ = TRANSLATE;
		}
		viewport()->setCursor(Qt::OpenHandCursor);
	} else {
		QGraphicsView::keyPressEvent(event);
	}
}

void EditorView::keyReleaseEvent(QKeyEvent *event) {
	if(event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
		event->accept();
		dragbtndown_ = NOTRANSFORM;
		if(isdragging_==NOTRANSFORM)
			viewport()->setCursor(cursor_);
	} else {
		QGraphicsView::keyReleaseEvent(event);
	}
}
//! Handle viewport events
/**
 * Tablet events are handled here
 * @param event event info
 */
bool EditorView::viewportEvent(QEvent *event)
{
	if(event->type() == QEvent::TabletMove) {
		// Stylus moved
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		tabev->accept();
		const dpcore::Point point(mapToScene(tabev->pos()), tabev->pressure());

		if(!prevpoint_.intSame(point)) {
			if(isdragging_)
				moveDrag(tabev->x(), tabev->y());
			else {
				if(pendown_) {
					if(point.pressure()==0) {
						// Missed a release event
						pendown_ = NOTDOWN;
						emit penUp();
					} else {
						emit penMove(point);
					}
				}
				updateOutline(point);
			}
			prevpoint_ = point;
		}
	} else if(event->type() == QEvent::TabletPress) {
		// Stylus touches the tablet surface
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		tabev->accept();
		if(dragbtndown_) {
			startDrag(tabev->x(), tabev->y(), dragbtndown_);
		} else {
			if(pendown_ == NOTDOWN) {
				const dpcore::Point point(mapToScene(tabev->pos()), tabev->pressure());

				pendown_ = TABLETDOWN;
				emit penDown(point);
				updateOutline(point);
				prevpoint_ = point;
			}
		}
	} else if(event->type() == QEvent::TabletRelease) {
		// Stylus lifted
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		tabev->accept();
		if(isdragging_) {
			stopDrag();
		} else if(pendown_ == TABLETDOWN) {
			dpcore::Point point(mapToScene(tabev->pos()), 0);
			updateOutline(point);
			prevpoint_ = point;
			pendown_ = NOTDOWN;
			emit penUp();
		}
	} else {
		return QGraphicsView::viewportEvent(event);
	}
	
	return true;
}

void EditorView::updateOutline(const dpcore::Point& point) {
	if(enableoutline_ && showoutline_) {
		QList<QRectF> rect;
		rect.append(QRectF(prevpoint_.x() - outlinesize_,
					prevpoint_.y() - outlinesize_, dia_, dia_));
		rect.append(QRectF(point.x() - outlinesize_,
					point.y() - outlinesize_, dia_, dia_));
		updateScene(rect);
	}
}

void EditorView::sceneChanged()
{
	// Signal visible view rectangle change
	emit viewMovedTo(mapToScene(rect()).boundingRect());
}

/**
 * @param x initial x coordinate
 * @param y initial y coordinate
 */
void EditorView::startDrag(int x,int y, ViewTransform mode)
{
	viewport()->setCursor(Qt::ClosedHandCursor);
	dragx_ = x;
	dragy_ = y;
	isdragging_ = mode;
	if(enableoutline_) {
		showoutline_ = false;
		QList<QRectF> rect;
		rect.append(QRectF(prevpoint_.x() - outlinesize_,
					prevpoint_.y() - outlinesize_,
					dia_, dia_));
		updateScene(rect);
	}
}

void EditorView::scrollTo(const QPoint& point)
{
	centerOn(point);
	// notify of scene change
	sceneChanged();
}

/**
 * @param x x coordinate
 * @param y y coordinate
 */
void EditorView::moveDrag(int x, int y)
{
	const int dx = dragx_ - x;
	const int dy = dragy_ - y;

	if(isdragging_==ROTATE) {
		qreal preva = atan2( width()/2 - dragx_, height()/2 - dragy_ );
		qreal a = atan2( width()/2 - x, height()/2 - y );
		rotate((preva-a) * (180.0 / M_PI));
	} else {
		QScrollBar *ver = verticalScrollBar();
		ver->setSliderPosition(ver->sliderPosition()+dy);
		QScrollBar *hor = horizontalScrollBar();
		hor->setSliderPosition(hor->sliderPosition()+dx);
	}

	dragx_ = x;
	dragy_ = y;

	// notify of scene change
	sceneChanged();
}

//! Stop dragging
void EditorView::stopDrag()
{
	if(dragbtndown_ != NOTRANSFORM)
		viewport()->setCursor(Qt::OpenHandCursor);
	else
		viewport()->setCursor(cursor_);
	isdragging_ = NOTRANSFORM;
	showoutline_ = true;
}

/**
 * @brief accept image drops
 * @param event event info
 *
 * @todo Check file extensions
 */
void EditorView::dragEnterEvent(QDragEnterEvent *event)
{
	if(event->mimeData()->hasUrls())
		event->acceptProposedAction();
}

/**
 * @brief handle color and image drops
 * @param event event info
 *
 * @todo Reset the image modification state to unmodified
 */
void EditorView::dropEvent(QDropEvent *event)
{
	emit imageDropped(event->mimeData()->urls().first().toLocalFile());
	event->acceptProposedAction();
}

}
