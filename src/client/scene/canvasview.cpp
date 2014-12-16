/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <QMouseEvent>
#include <QTabletEvent>
#include <QScrollBar>
#include <QUrl>
#include <QBitmap>
#include <QPainter>
#include <QMimeData>
#include <QApplication>
#include <QGestureEvent>

// Qt 5.0 compatibility. Remove once Qt 5.1 ships on mainstream distros
#if (QT_VERSION < QT_VERSION_CHECK(5, 1, 0))
#include <cmath>
#define qAtan2 atan2
inline float qRadiansToDegrees(float radians) {
	return radians * float(180/M_PI);
}
#define qFloor floor
#else
#include <QtMath>
#endif

#include "canvasview.h"
#include "canvasscene.h"
#include "docks/toolsettingsdock.h"
#include "tools/toolsettings.h"

#include "net/client.h"
#include "core/point.h"

namespace widgets {

CanvasView::CanvasView(QWidget *parent)
	: QGraphicsView(parent), _pendown(NOTDOWN), _specialpenmode(false), _isdragging(DRAG_NOTRANSFORM),
	_dragbtndown(DRAG_NOTRANSFORM), _outlinesize(0),
	_enableoutline(true), _showoutline(true), _zoom(100), _rotate(0), _scene(0),
	_smoothing(0), _pressuremode(PRESSUREMODE_STYLUS),
	_zoomWheelDelta(0),
	_locked(false), _pointertracking(false), _enableTabletEvents(true)
{
	viewport()->setAcceptDrops(true);
	viewport()->grabGesture(Qt::PinchGesture);
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

	// Get the color picker cursor
	_colorpickcursor = QCursor(QPixmap(":/cursors/colorpicker.png"), 2, 29);
}

void CanvasView::enableTabletEvents(bool enable)
{
	_enableTabletEvents = enable;
}

void CanvasView::setCanvas(drawingboard::CanvasScene *scene)
{
	_scene = scene;
	_toolbox.setScene(scene);
	setScene(scene);

	connect(_scene, &drawingboard::CanvasScene::canvasInitialized, [this]() {
		viewRectChanged();
	});

	connect(_scene, &drawingboard::CanvasScene::canvasResized, [this](int xoff, int yoff, const QSize &oldsize) {
		if(oldsize.isEmpty()) {
			centerOn(_scene->sceneRect().center());
		} else {
			scrollContentsBy(-xoff, -yoff);
		}
	});

}

void CanvasView::setClient(net::Client *client)
{
	_toolbox.setClient(client);
	connect(this, SIGNAL(pointerMoved(QPointF)), client, SLOT(sendLaserPointer(QPointF)));
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
	if(_locked) {
		viewport()->setCursor(Qt::ForbiddenCursor);

	} else {
		const QCursor &c = _current_tool->cursor();
		// We use our own custom cross cursor instead of the standard one
		if(c.shape() == Qt::CrossCursor)
			viewport()->setCursor(_cursor);
		else
			viewport()->setCursor(c);
	}
}

void CanvasView::selectTool(tools::Type tool)
{
	_current_tool = _toolbox.get(tool);
	resetCursor();
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
void CanvasView::setOutlineSize(int size)
{
	float updatesize = _outlinesize;
	_outlinesize = size;
	if(_enableoutline && _showoutline) {
		if(_outlinesize>updatesize)
			updatesize = _outlinesize;
		QList<QRectF> rect;
		rect.append(QRectF(_prevoutlinepoint.x() - updatesize/2,
					_prevoutlinepoint.y() - updatesize/2,
					updatesize, updatesize));
		updateScene(rect);
	}
}

void CanvasView::setOutlineSubpixelMode(bool subpixel)
{
	_subpixeloutline = subpixel;
}

void CanvasView::drawForeground(QPainter *painter, const QRectF& rect)
{
	if(_enableoutline && _showoutline && _outlinesize>1 && !_locked) {
		const QRectF outline(_prevoutlinepoint-QPointF(_outlinesize/2,_outlinesize/2),
					QSizeF(_outlinesize, _outlinesize));
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
	// using spacebar for dragging works rightaway. Avoid stealing
	// focus from text edit widgets though.
	QWidget *oldfocus = QApplication::focusWidget();
	if(!oldfocus || !(
		oldfocus->inherits("QLineEdit") ||
		oldfocus->inherits("QTextEdit") ||
		oldfocus->inherits("QPlainTextEdit"))
		) {
		setFocus(Qt::MouseFocusReason);
	}
}

void CanvasView::leaveEvent(QEvent *event)
{
	QGraphicsView::leaveEvent(event);
	if(_enableoutline) {
		_showoutline = false;
		updateOutline();
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
				_current_tool->begin(p, right, _zoom);
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
						_current_tool->begin(_smoother.smoothPoint(), right, _zoom);
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
				_current_tool->begin(_smoother.latestPoint(), right, _zoom);
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
			else if((event->modifiers() & Qt::ShiftModifier))
				mode = DRAG_QUICKADJUST1;
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
		updateOutline(point);
		if(!_prevpoint.intSame(point)) {
			if(_pendown) {
				_pointervelocity = point.distance(_prevpoint);
				_pointerdistance += _pointervelocity;
				point.setPressure(mapPressure(1.0, false));
				onPenMove(point, event->buttons() & Qt::RightButton, event->modifiers() & Qt::ShiftModifier, event->modifiers() & Qt::AltModifier);
			} else if(_pointertracking && _scene->hasImage()) {
				emit pointerMoved(point);
			}
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
		_zoomWheelDelta += event->angleDelta().y();
		int steps=_zoomWheelDelta / 120;
		_zoomWheelDelta -= steps * 120;

		if(steps != 0) {
			if(_zoom<100 || (_zoom==100 && steps<0))
				setZoom(qRound((_zoom + steps * 10) / 10) * 10);
			else
				setZoom(qRound((_zoom + steps * 50) / 50) * 50);
		}

	} else if((event->modifiers() & Qt::ShiftModifier)) {
		doQuickAdjust1(event->angleDelta().y() / (30 * 4.0));

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

		if(event->key() == Qt::Key_Control && !_dragbtndown)
			viewport()->setCursor(_colorpickcursor);

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

		if(event->key() == Qt::Key_Control) {
			if(_dragbtndown)
				viewport()->setCursor(Qt::OpenHandCursor);
			else
				resetCursor();
		}
	}
}

void CanvasView::gestureEvent(QGestureEvent *event)
{
	QPinchGesture *pinch = static_cast<QPinchGesture*>(event->gesture(Qt::PinchGesture));
	if(pinch) {
		if(pinch->state() == Qt::GestureStarted) {
			_gestureStartZoom = _zoom;
			_gestureStartAngle = _rotate;
		}

		if((pinch->changeFlags() & QPinchGesture::ScaleFactorChanged))
			setZoom(_gestureStartZoom * pinch->scaleFactor());

		if((pinch->changeFlags() & QPinchGesture::RotationAngleChanged))
			setRotation(_gestureStartAngle + pinch->rotationAngle());
	}
}

//! Handle viewport events
/**
 * Tablet events are handled here
 * @param event event info
 */
bool CanvasView::viewportEvent(QEvent *event)
{
	if(event->type() == QEvent::Gesture) {
		gestureEvent(static_cast<QGestureEvent*>(event));

	} else if(event->type() == QEvent::TabletMove && _enableTabletEvents) {
		// Stylus moved
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		tabev->accept();

		paintcore::Point point = mapToScene(tabev->posF(), tabev->pressure());
		updateOutline(point);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
		if(!_isdragging && tabev->buttons() & Qt::MidButton) {
			ViewTransform mode;

			// tablet event modifiers still don't seem to work in Qt 5.4.0
			if((tabev->modifiers() & Qt::ControlModifier))
				mode = DRAG_ZOOM;
			else if((tabev->modifiers() & Qt::ShiftModifier))
				mode = DRAG_QUICKADJUST1;
			else
				mode = DRAG_TRANSLATE;

			startDrag(tabev->x(), tabev->y(), mode);

		} else {
#endif
			if(!_prevpoint.intSame(point)) {
				if(_isdragging)
					moveDrag(tabev->x(), tabev->y());
				else {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
					const bool rightbutton = tabev->buttons() & Qt::RightButton;
#else
					const bool rightbutton = false;
#endif
					if(_pendown) {
						if(_prevpoint.x() == -999) {
							// start of a new stroke
							_prevpoint = point;
							onPenDown(point, rightbutton);
						} else {
							_pointervelocity = point.distance(_prevpoint);
							_pointerdistance += _pointervelocity;
							point.setPressure(mapPressure(point.pressure(), true));
							onPenMove(point, rightbutton, tabev->modifiers() & Qt::ShiftModifier, tabev->modifiers() & Qt::AltModifier);
						}
					}
				}
				_prevpoint = point;
			}
#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
		}
#endif

	} else if(event->type() == QEvent::TabletPress && _enableTabletEvents) {
		// Stylus touches the tablet surface
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		tabev->accept();
		if(_dragbtndown) {
			startDrag(tabev->x(), tabev->y(), _dragbtndown);
		} else {
			if(_pendown == NOTDOWN) {
				_pointerdistance = 0;
				_pointervelocity = 0;

				_specialpenmode = tabev->modifiers() & Qt::ControlModifier; /* note: modifiers doesn't seem to work, at least on Qt 5.2.0 */
				_pendown = TABLETDOWN;

				// don't call onPenDown here, because sometimes the position seems to be wildly off here.
				_prevpoint = paintcore::Point(-999, -999, 0);
			}
		}
	} else if(event->type() == QEvent::TabletRelease && _enableTabletEvents) {
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
	switch(_pressuremode) {
	case PRESSUREMODE_STYLUS:
		return stylus ? _pressurecurve.value(pressure) : 1.0;

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

void CanvasView::updateOutline(paintcore::Point point) {
	if(!_subpixeloutline) {
		point.setX(qFloor(point.x()) + 0.5);
		point.setY(qFloor(point.y()) + 0.5);
	}
	if(_enableoutline && _showoutline && !_locked && !point.roughlySame(_prevoutlinepoint)) {
		QList<QRectF> rect;
		const float oR = _outlinesize / 2;
		rect.append(QRectF(
					_prevoutlinepoint.x() - oR,
					_prevoutlinepoint.y() - oR,
					_outlinesize,
					_outlinesize
				));
		rect.append(QRectF(
						point.x() - oR,
						point.y() - oR,
						_outlinesize,
						_outlinesize
					));
		updateScene(rect);
		_prevoutlinepoint = point;
	}
}

void CanvasView::updateOutline()
{
	QList<QRectF> rect;
	rect.append(QRectF(_prevoutlinepoint.x() - _outlinesize/2,
				_prevoutlinepoint.y() - _outlinesize/2,
				_outlinesize, _outlinesize));
	updateScene(rect);

}

void CanvasView::doQuickAdjust1(float delta)
{
	// Brush attribute adjustment is allowed only when stroke is not in progress
	if(_pendown == NOTDOWN)
		_toolbox.toolsettings()->quickAdjustCurrent1(delta);
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
		updateOutline();
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
	} else if(_isdragging==DRAG_QUICKADJUST1) {
		if(dy!=0) {
			float delta = qBound(-2.0, dy / 10.0, 2.0);
			doQuickAdjust1(delta);
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
	if(event->mimeData()->hasUrls() || event->mimeData()->hasImage() || event->mimeData()->hasColor())
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
	} else if(data->hasColor()) {
		emit colorDropped(event->mimeData()->colorData().value<QColor>());
	} else {
		// unsupported data
		return;
	}
	event->acceptProposedAction();
}

}
