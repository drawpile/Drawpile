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
#include "docks/toolsettingsdock.h"
#include "toolsettings.h"

#include "net/client.h"
#include "core/point.h"

namespace widgets {

CanvasView::CanvasView(QWidget *parent)
	: QGraphicsView(parent), _pendown(NOTDOWN), _specialpenmode(false), _isdragging(DRAG_NOTRANSFORM),
	_dragbtndown(DRAG_NOTRANSFORM), _outlinesize(10), _dia(20),
	_enableoutline(true), _showoutline(true), _zoom(100), _rotate(0), _scene(0),
	_smoothing(0), _pressuremode(PRESSUREMODE_NONE), _usestylus(true),
	_locked(false), _pointertracking(false)
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
	connect(this, SIGNAL(pointerMoved(int,int)), client, SLOT(sendPointerMove(int,int)));
}

void CanvasView::setToolSettings(docks::ToolSettings *settings)
{
	_toolbox.setToolSettings(settings);
	connect(settings->getLaserPointerSettings(), SIGNAL(pointerTrackingToggled(bool)), this, SLOT(setPointerTracking(bool)));
}

void CanvasView::zoomin()
{
	setZoom(_zoom * 2);
}

void CanvasView::zoomout()
{
	setZoom(_zoom / 2);
}

/**
 * You should use this function instead of calling scale() directly
 * to keep track of the zoom factor.
 * @param zoom new zoom factor
 */
void CanvasView::setZoom(qreal zoom)
{
	if(zoom<=0)
		return;

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
	qreal xf = qAbs(modf(point.x(), &tmp));
	qreal yf = qAbs(modf(point.y(), &tmp));

	QPoint p0(floor(point.x()), floor(point.y()));
	QPointF p1 = mapToScene(p0);
	QPointF p2 = mapToScene(p0 + QPoint(1,1));

	QPointF mapped(
		(p1.x()-p2.x()) * xf + p2.x(),
		(p1.y()-p2.y()) * yf + p2.y()
	);

	return paintcore::Point(mapped, pressure);
}

void CanvasView::setPointerTracking(bool tracking)
{
	_pointertracking = tracking;
	if(!tracking && _scene) {
		_scene->hideUserMarker();
	}
}

void CanvasView::setStrokeSmoothing(int smoothing)
{
	Q_ASSERT(smoothing>=0);
	_smoothing = smoothing;
	if(smoothing>0)
		_smoother.setSmoothing(smoothing);
}

void CanvasView::setPressureMode(PressureMode mode, float param)
{
	_pressuremode = PressureMode(mode);
	_modeparam = param;
}

void CanvasView::setStylusPressureEnabled(bool enabled)
{
	_usestylus = enabled;
}

void CanvasView::setPressureCurve(const KisCubicCurve &curve)
{
	_pressurecurve = curve;
}

void CanvasView::setDistanceCurve(const KisCubicCurve &curve)
{
	_pressuredistance = curve;
}

void CanvasView::setVelocityCurve(const KisCubicCurve &curve)
{
	_pressurevelocity = curve;
}

void CanvasView::onPenDown(const paintcore::Point &p, bool right)
{
	if(_scene->hasImage() && !_locked) {

		if(_specialpenmode) {
			// quick color pick mode
			_scene->pickColor(p.x(), p.y(), 0, right);
		} else {
			if(_smoothing>0 && _current_tool->allowSmoothing())
				_smoother.addPoint(p);
			else
				_current_tool->begin(p, right);
		}
	}
}

void CanvasView::onPenMove(const paintcore::Point &p, bool right, bool shift, bool alt)
{
	if(_scene->hasImage() && !_locked) {
		if(_specialpenmode) {
			// quick color pick mode
			_scene->pickColor(p.x(), p.y(), 0, right);
		} else {
			if(_smoothing>0 && _current_tool->allowSmoothing()) {
				_smoother.addPoint(p);
				if(_smoother.hasSmoothPoint()) {
					if(_smoother.isFirstSmoothPoint())
						_current_tool->begin(_smoother.smoothPoint(), right);
					else
						_current_tool->motion(_smoother.smoothPoint(), shift, alt);
				}
			} else {
				_current_tool->motion(p, shift, alt);
			}
		}
	}
}

void CanvasView::onPenUp(bool right)
{
	if(_scene->hasImage() && !_locked) {
		if(!_specialpenmode) {
			// Add the missing single dab when smoothing is used
			if(_smoothing>0 && _current_tool->allowSmoothing() && !_smoother.hasSmoothPoint()) {
				_current_tool->begin(_smoother.latestPoint(), right);
			}
			_current_tool->end();
		}

		_smoother.reset();
	}
}

//! Handle mouse press events
void CanvasView::mousePressEvent(QMouseEvent *event)
{
	/** @todo why do we sometimes get mouse events for tablet strokes? */
	if(_pendown != NOTDOWN)
		return;
	if(event->button() == Qt::MidButton || _dragbtndown) {
		ViewTransform mode;
		if(_dragbtndown == DRAG_NOTRANSFORM) {
			if((event->modifiers() & Qt::ControlModifier))
				mode = DRAG_ZOOM;
			else
				mode = DRAG_TRANSLATE;
		} else
			mode = _dragbtndown;

		startDrag(event->x(), event->y(), mode);
	} else if((event->button() == Qt::LeftButton || event->button() == Qt::RightButton) && _isdragging==DRAG_NOTRANSFORM) {
		_pendown = MOUSEDOWN;
		_pointerdistance = 0;
		_pointervelocity = 0;

		_specialpenmode = event->modifiers() & Qt::ControlModifier;
		onPenDown(mapToScene(event->pos(), mapPressure(1.0, false)), event->button() == Qt::RightButton);
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
		paintcore::Point point = mapToScene(event->pos(), 1);
		if(!_prevpoint.intSame(point)) {
			if(_pendown) {
				_pointervelocity = point.distance(_prevpoint);
				_pointerdistance += _pointervelocity;
				point.setPressure(mapPressure(1.0, false));
				onPenMove(point, event->button() == Qt::RightButton, event->modifiers() & Qt::ShiftModifier, event->modifiers() & Qt::AltModifier);
			} else if(_pointertracking && _scene->hasImage()) {
				emit pointerMoved(point.x(), point.y());
			}
			updateOutline(point);
			_prevpoint = point;
		}
	}
}

//! Handle mouse release events
void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
	_prevpoint = mapToScene(event->pos(), 0.0);
	if(_isdragging) {
		stopDrag();
	} else if(_pendown == TABLETDOWN || ((event->button() == Qt::LeftButton || event->button() == Qt::RightButton) && _pendown == MOUSEDOWN)) {
		_pendown = NOTDOWN;
		onPenUp(event->button() == Qt::RightButton);
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
			_dragbtndown = DRAG_ROTATE;
		} else {
			_dragbtndown = DRAG_TRANSLATE;
		}
		viewport()->setCursor(Qt::OpenHandCursor);
	} else {
		QGraphicsView::keyPressEvent(event);
	}
}

void CanvasView::keyReleaseEvent(QKeyEvent *event) {
	if(event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
		event->accept();
		_dragbtndown = DRAG_NOTRANSFORM;
		if(_isdragging==DRAG_NOTRANSFORM)
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

		paintcore::Point point = mapToScene(tabev->posF(), tabev->pressure());

		if(!_prevpoint.intSame(point)) {
			if(_isdragging)
				moveDrag(tabev->x(), tabev->y());
			else {
				if(_pendown) {
					_pointervelocity = point.distance(_prevpoint);
					_pointerdistance += _pointervelocity;
					point.setPressure(mapPressure(point.pressure(), true));
					onPenMove(point, false, tabev->modifiers() & Qt::ShiftModifier, tabev->modifiers() & Qt::AltModifier);
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
				_pointerdistance = 0;
				_pointervelocity = 0;

				const paintcore::Point point = mapToScene(tabev->posF(), mapPressure(tabev->pressure(), true));

				_specialpenmode = tabev->modifiers() & Qt::ControlModifier; /* note: modifiers doesn't seem to work, at least on Qt 5.2.0 */
				_pendown = TABLETDOWN;
				onPenDown(point, false);
				updateOutline(point);
				_prevpoint = point;

			}
		}
	} else if(event->type() == QEvent::TabletRelease) {
		// Stylus lifted
		// Ignore this event: a mouseRelease event is also generated, so we let
		// the mouseRleaseEvent function handle this.
	} else {
		return QGraphicsView::viewportEvent(event);
	}
	
	return true;
}

float CanvasView::mapPressure(float pressure, bool stylus)
{
	if(stylus && _usestylus) {
		// Use real pressure
		return _pressurecurve.value(pressure);
	}

	// Otherwise fake pressure
	switch(_pressuremode) {
	case PRESSUREMODE_NONE:
		return 1.0;

	case PRESSUREMODE_DISTANCE: {
		float d = qMin(_pointerdistance, _modeparam) / _modeparam;
		return _pressuredistance.value(d);
	}

	case PRESSUREMODE_VELOCITY:
		float v = qMin(_pointervelocity, _modeparam) / _modeparam;
		return _pressurevelocity.value(v);
	}

	// Shouldn't be reached
	Q_ASSERT(false);
	return 0;
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

QPoint CanvasView::viewCenterPoint() const
{
	return mapToScene(rect().center()).toPoint();
}

void CanvasView::viewRectChanged()
{
	// Signal visible view rectangle change
	emit viewRectChange(mapToScene(rect()));
}

void CanvasView::scrollTo(const QPoint& point)
{
	centerOn(point);
}

/**
 * @param x initial x coordinate
 * @param y initial y coordinate
 * @param mode dragging mode
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

/**
 * @param x x coordinate
 * @param y y coordinate
 */
void CanvasView::moveDrag(int x, int y)
{
	const int dx = _dragx - x;
	const int dy = _dragy - y;

	if(_isdragging==DRAG_ROTATE) {
		qreal preva = qAtan2( width()/2 - _dragx, height()/2 - _dragy );
		qreal a = qAtan2( width()/2 - x, height()/2 - y );
		setRotation(rotation() + qRadiansToDegrees(preva-a));

	} else if(_isdragging==DRAG_ZOOM) {
		if(dy!=0) {
			float delta = qBound(-1.0, dy / 100.0, 1.0);
			if(delta>0) {
				setZoom(_zoom * (1+delta));
			} else if(delta<0) {
				setZoom(_zoom / (1-delta));
			}
		}

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
	if(_dragbtndown != DRAG_NOTRANSFORM)
		viewport()->setCursor(Qt::OpenHandCursor);
	else
		resetCursor();
	_isdragging = DRAG_NOTRANSFORM;
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
