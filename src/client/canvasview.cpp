/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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
#include <QMimeData>

// Qt 5.0 compatibility. Remove once Qt 5.1 ships on mainstream distros
#if (QT_VERSION < QT_VERSION_CHECK(5, 1, 0))
#include <cmath>
#define qAtan2 atan2
inline float qRadiansToDegrees(float radians) {
	return radians * float(180/M_PI);
}
#else
#include <QtMath>
#endif

#include "canvasview.h"
#include "canvasscene.h"

#include "core/point.h"

namespace widgets {

CanvasView::CanvasView(QWidget *parent)
	: QGraphicsView(parent), _pendown(NOTDOWN), _isdragging(NOTRANSFORM),
	_dragbtndown(NOTRANSFORM), _outlinesize(10), _dia(20),
	_enableoutline(true), _showoutline(true), _zoom(100), _rotate(0), _scene(0),
	_smoothing(0), _locked(false)
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
	_cursor = QCursor(bm, mask);
	viewport()->setCursor(_cursor);
}

void CanvasView::setCanvas(drawingboard::CanvasScene *scene)
{
	_scene = scene;
	_toolbox.setScene(scene);
	setScene(scene);

	connect(_scene, &drawingboard::CanvasScene::canvasInitialized, [this]() { viewRectChanged(); });
}

void CanvasView::setClient(net::Client *client)
{
	_toolbox.setClient(client);
}

void CanvasView::setToolSettings(widgets::ToolSettingsDock *settings)
{
	_toolbox.setToolSettings(settings);
}

/**
 * You should use this function instead of calling scale() directly
 * to keep track of the zoom factor.
 * @param zoom new zoom factor
 */
void CanvasView::setZoom(int zoom)
{
	Q_ASSERT(zoom>0);
	_zoom = zoom;
	QMatrix nm(1,0,0,1, matrix().dx(), matrix().dy());
	nm.scale(_zoom/100.0, _zoom/100.0);
	nm.rotate(_rotate);
	setMatrix(nm);
	emit viewTransformed(_zoom, _rotate);
	viewRectChanged();
}

/**
 * You should use this function instead calling rotate() directly
 * to keep track of the rotation angle.
 * @param angle new rotation angle
 */
void CanvasView::setRotation(qreal angle)
{
	_rotate = angle;
	setZoom(_zoom);
}

void CanvasView::setLocked(bool lock)
{
	_locked = lock;
	resetCursor();
}

void CanvasView::resetCursor()
{
	if(_locked)
		viewport()->setCursor(Qt::ForbiddenCursor);
	else
		viewport()->setCursor(_cursor);
}

void CanvasView::selectTool(tools::Type tool)
{
	_current_tool = _toolbox.get(tool);
}

void CanvasView::selectLayer(int layer_id)
{
	_toolbox.selectLayer(layer_id);
}

/**
 * @param enable if true, brush outline is shown
 */
void CanvasView::setOutline(bool enable)
{
	_enableoutline = enable;
	viewport()->setMouseTracking(enable);
}

/**
 * @param radius circle radius
 */
void CanvasView::setOutlineRadius(int radius)
{
	int updatesize = _outlinesize;
	_outlinesize = radius;
	_dia = radius*2;
	if(_enableoutline && _showoutline) {
		if(_outlinesize>updatesize)
			updatesize = _outlinesize;
		QList<QRectF> rect;
		rect.append(QRectF(_prevpoint.x() - updatesize,
					_prevpoint.y() - updatesize,
					updatesize*2, updatesize*2));
		updateScene(rect);
	}
}

void CanvasView::drawForeground(QPainter *painter, const QRectF& rect)
{
	if(_enableoutline && _showoutline && _outlinesize>1 && !_locked) {
		const QRectF outline(_prevpoint-QPointF(_outlinesize,_outlinesize),
					QSizeF(_dia, _dia));
		if(rect.intersects(outline)) {
			painter->save();
			QPen pen(Qt::white);
			pen.setCosmetic(true);
			painter->setPen(pen);
			painter->setCompositionMode(QPainter::CompositionMode_Difference);
			painter->drawEllipse(outline);
			painter->restore();
		}
	}
}

void CanvasView::enterEvent(QEvent *event)
{
	QGraphicsView::enterEvent(event);
	if(_enableoutline) {
		_showoutline = true;
	}
	// Give focus to this widget on mouseover. This is so that
	// using spacebar for dragging works rightaway.
	setFocus(Qt::MouseFocusReason);
}

void CanvasView::leaveEvent(QEvent *event)
{
	QGraphicsView::leaveEvent(event);
	if(_enableoutline) {
		_showoutline = false;
		QList<QRectF> rect;
		rect.append(QRectF(_prevpoint.x() - _outlinesize,
					_prevpoint.y() - _outlinesize,
					_dia, _dia));
		updateScene(rect);
	}
}

void CanvasView::scrollContentsBy(int dx, int dy)
{
	QGraphicsView::scrollContentsBy(dx, dy);
	viewRectChanged();
}

void CanvasView::resizeEvent(QResizeEvent *event)
{
	QGraphicsView::resizeEvent(event);
	viewRectChanged();
}

paintcore::Point CanvasView::mapToScene(const QPoint &point, qreal pressure) const
{
	return paintcore::Point(mapToScene(point), pressure);
}

paintcore::Point CanvasView::mapToScene(const QPointF &point, qreal pressure) const
{
	// QGraphicsView API lacks mapToScene(QPointF), even though
	// the QPoint is converted to QPointF internally...
	// To work around this, map (x,y) and (x+1, y+1) and linearly interpolate
	// between the two
	double tmp;
	qreal xf = modf(point.x(), &tmp);
	qreal yf = modf(point.y(), &tmp);

	QPoint p0 = point.toPoint();
	QPointF p1 = mapToScene(p0);
	QPointF p2 = mapToScene(p0 + QPoint(1,1));

	QPointF mapped(
		(p1.x()-p2.x()) * xf + p2.x(),
		(p1.y()-p2.y()) * yf + p2.y()
	);

	return paintcore::Point(mapped, pressure);
}

//! Handle mouse press events
void CanvasView::mousePressEvent(QMouseEvent *event)
{
	/** @todo why do we sometimes get mouse events for tablet strokes? */
	if(_pendown != NOTDOWN)
		return;
	if(event->button() == Qt::MidButton || _dragbtndown) {
		startDrag(event->x(), event->y(), _dragbtndown!=ROTATE?TRANSLATE:ROTATE);
	} else if((event->button() == Qt::LeftButton || event->button() == Qt::RightButton) && _isdragging==NOTRANSFORM) {
		_pendown = MOUSEDOWN;
		
		onPenDown(mapToScene(event->pos(), 1.0), event->button() == Qt::RightButton);
	}
}

//! Handle mouse motion events
void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
	/** @todo why do we sometimes get mouse events for tablet strokes? */
	if(_pendown == TABLETDOWN)
		return;
	if(_pendown && event->buttons() == Qt::NoButton) {
		// In case we missed a mouse release
		mouseReleaseEvent(event);
		return;
	}

	if(_isdragging) {
		moveDrag(event->x(), event->y());
	} else {
		const paintcore::Point point = mapToScene(event->pos(), 1.0);
		if(!_prevpoint.intSame(point)) {
			if(_pendown)
				onPenMove(point, event->button() == Qt::RightButton);
			updateOutline(point);
			_prevpoint = point;
		}
	}
}

void CanvasView::setStrokeSmoothing(int smoothing)
{
	Q_ASSERT(smoothing>=0);
	_smoothing = smoothing;
	if(smoothing>0)
		_smoother.setSmoothing(smoothing);
}

void CanvasView::onPenDown(const paintcore::Point &p, bool right)
{
	if(_scene->hasImage() && !_locked) {
		if(_smoothing>0)
			_smoother.addPoint(p);
		else
			_current_tool->begin(p, right);

	}
}

void CanvasView::onPenMove(const paintcore::Point &p, bool right)
{
	if(_scene->hasImage() && !_locked) {
		if(_smoothing>0) {
			_smoother.addPoint(p);
			if(_smoother.hasSmoothPoint()) {
				if(_smoother.isFirstSmoothPoint())
					_current_tool->begin(_smoother.smoothPoint(), right);
				else
					_current_tool->motion(_smoother.smoothPoint());
			}
		} else {
			_current_tool->motion(p);
		}
	}
}

void CanvasView::onPenUp()
{
	if(_scene->hasImage() && !_locked) {
		_smoother.reset();
		_current_tool->end();
	}
}

//! Handle mouse release events
void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
	if(_pendown == TABLETDOWN)
		return;
	_prevpoint = mapToScene(event->pos(), 0.0);
	if(_isdragging) {
		stopDrag();
	} else if((event->button() == Qt::LeftButton || event->button() == Qt::RightButton) && _pendown == MOUSEDOWN) {
		_pendown = NOTDOWN;
		onPenUp();
	}
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent*)
{
	// Ignore doubleclicks
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
	if((event->modifiers() & Qt::ControlModifier)) {
		float delta = event->angleDelta().y() / (30 * 8.0);
		if(delta>0) {
			setZoom(_zoom * (1+delta));
		} else if(delta<0) {
			setZoom(_zoom / (1-delta));
		}
	} else {
		QGraphicsView::wheelEvent(event);
	}
}

void CanvasView::keyPressEvent(QKeyEvent *event) {
	if(event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
		event->accept();
		if(event->modifiers() & Qt::ControlModifier) {
			_dragbtndown = ROTATE;
		} else {
			_dragbtndown = TRANSLATE;
		}
		viewport()->setCursor(Qt::OpenHandCursor);
	} else {
		QGraphicsView::keyPressEvent(event);
	}
}

void CanvasView::keyReleaseEvent(QKeyEvent *event) {
	if(event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
		event->accept();
		_dragbtndown = NOTRANSFORM;
		if(_isdragging==NOTRANSFORM)
			resetCursor();
	} else {
		QGraphicsView::keyReleaseEvent(event);
	}
}
//! Handle viewport events
/**
 * Tablet events are handled here
 * @param event event info
 */
bool CanvasView::viewportEvent(QEvent *event)
{
	if(event->type() == QEvent::TabletMove) {
		// Stylus moved
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		tabev->accept();
		const paintcore::Point point = mapToScene(tabev->posF(), tabev->pressure());

		if(!_prevpoint.intSame(point)) {
			if(_isdragging)
				moveDrag(tabev->x(), tabev->y());
			else {
				if(_pendown) {
					if(point.pressure()==0) {
						// Missed a release event
						_pendown = NOTDOWN;
						onPenUp();
					} else {
						onPenMove(point, false);
					}
				}
				updateOutline(point);
			}
			_prevpoint = point;
		}
	} else if(event->type() == QEvent::TabletPress) {
		// Stylus touches the tablet surface
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		tabev->accept();
		if(_dragbtndown) {
			startDrag(tabev->x(), tabev->y(), _dragbtndown);
		} else {
			if(_pendown == NOTDOWN) {
				const paintcore::Point point = mapToScene(tabev->posF(), tabev->pressure());

				_pendown = TABLETDOWN;
				onPenDown(point, false);
				updateOutline(point);
				_prevpoint = point;
			}
		}
	} else if(event->type() == QEvent::TabletRelease) {
		// Stylus lifted
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		tabev->accept();
		if(_isdragging) {
			stopDrag();
		} else if(_pendown == TABLETDOWN) {
			paintcore::Point point = mapToScene(tabev->posF(), 0);
			updateOutline(point);
			_prevpoint = point;
			_pendown = NOTDOWN;
			onPenUp();
		}
	} else {
		return QGraphicsView::viewportEvent(event);
	}
	
	return true;
}

void CanvasView::updateOutline(const paintcore::Point& point) {
	if(_enableoutline && _showoutline && !_locked) {
		QList<QRectF> rect;
		rect.append(QRectF(_prevpoint.x() - _outlinesize,
					_prevpoint.y() - _outlinesize, _dia, _dia));
		rect.append(QRectF(point.x() - _outlinesize,
					point.y() - _outlinesize, _dia, _dia));
		updateScene(rect);
	}
}

void CanvasView::viewRectChanged()
{
	// Signal visible view rectangle change
	emit viewRectChange(mapToScene(rect()));
}

/**
 * @param x initial x coordinate
 * @param y initial y coordinate
 */
void CanvasView::startDrag(int x,int y, ViewTransform mode)
{
	viewport()->setCursor(Qt::ClosedHandCursor);
	_dragx = x;
	_dragy = y;
	_isdragging = mode;
	if(_enableoutline) {
		_showoutline = false;
		QList<QRectF> rect;
		rect.append(QRectF(_prevpoint.x() - _outlinesize,
					_prevpoint.y() - _outlinesize,
					_dia, _dia));
		updateScene(rect);
	}
}

void CanvasView::scrollTo(const QPoint& point)
{
	centerOn(point);
}

/**
 * @param x x coordinate
 * @param y y coordinate
 */
void CanvasView::moveDrag(int x, int y)
{
	const int dx = _dragx - x;
	const int dy = _dragy - y;

	if(_isdragging==ROTATE) {
		qreal preva = qAtan2( width()/2 - _dragx, height()/2 - _dragy );
		qreal a = qAtan2( width()/2 - x, height()/2 - y );
		setRotation(rotation() + qRadiansToDegrees(preva-a));
	} else {
		QScrollBar *ver = verticalScrollBar();
		ver->setSliderPosition(ver->sliderPosition()+dy);
		QScrollBar *hor = horizontalScrollBar();
		hor->setSliderPosition(hor->sliderPosition()+dx);
	}

	_dragx = x;
	_dragy = y;
}

//! Stop dragging
void CanvasView::stopDrag()
{
	if(_dragbtndown != NOTRANSFORM)
		viewport()->setCursor(Qt::OpenHandCursor);
	else
		resetCursor();
	_isdragging = NOTRANSFORM;
	_showoutline = true;
}

/**
 * @brief accept image drops
 * @param event event info
 *
 * @todo Check file extensions
 */
void CanvasView::dragEnterEvent(QDragEnterEvent *event)
{
	if(event->mimeData()->hasUrls() || event->mimeData()->hasImage())
		event->acceptProposedAction();
}

void CanvasView::dragMoveEvent(QDragMoveEvent *event)
{
	if(event->mimeData()->hasUrls() || event->mimeData()->hasImage())
		event->acceptProposedAction();
}

/**
 * @brief handle image drops
 * @param event event info
 */
void CanvasView::dropEvent(QDropEvent *event)
{
	const QMimeData *data = event->mimeData();
	if(data->hasImage()) {
		emit imageDropped(qvariant_cast<QImage>(event->mimeData()->imageData()));
	} else if(data->hasUrls()) {
		emit urlDropped(event->mimeData()->urls().first());
	} else {
		// unsupported data
		return;
	}
	event->acceptProposedAction();
}

}
