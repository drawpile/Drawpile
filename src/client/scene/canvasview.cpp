/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2015 Calle Laakkonen

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
#include <QSettings>
#include <QWindow>
#include <QScreen>
#include <QtMath>

#include "canvasview.h"
#include "canvasscene.h"
#include "canvas/canvasmodel.h"
#include "tools/toolsettings.h"

#include "core/layerstack.h"
#include "core/point.h"
#include "notifications.h"

namespace widgets {

CanvasView::CanvasView(QWidget *parent)
	: QGraphicsView(parent), _pendown(NOTDOWN), _specialpenmode(false), _isdragging(DRAG_NOTRANSFORM),
	_dragbtndown(DRAG_NOTRANSFORM), _outlinesize(2),
	_showoutline(true), _enablecrosshair(true), _zoom(100), _rotate(0), _flip(false), _mirror(false), _scene(0),
	_tabletmode(ENABLE_TABLET),
	_zoomWheelDelta(0),
	_locked(false), _pointertracking(false), _pixelgrid(true),
	_hotBorderTop(false),
	_enableTouchScroll(true), _enableTouchPinch(true), _enableTouchTwist(true),
	_touching(false), _touchRotating(false),
	_dpi(96)
{
	viewport()->setAcceptDrops(true);
#ifdef Q_OS_MAC // Standard touch events seem to work better with mac touchpad
	viewport()->grabGesture(Qt::PinchGesture);
#else
	viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
#endif
	viewport()->setMouseTracking(true);
	setAcceptDrops(true);

	setBackgroundBrush(QColor(100,100,100));

	_crosshaircursor = QCursor(QPixmap(":/cursors/crosshair.png"), 15, 15);
	viewport()->setCursor(_crosshaircursor);

	// Get the color picker cursor
	_colorpickcursor = QCursor(QPixmap(":/cursors/colorpicker.png"), 2, 29);
}

void CanvasView::setTabletMode(TabletMode mode)
{
	_tabletmode = mode;
}

void CanvasView::setCanvas(drawingboard::CanvasScene *scene)
{
	_scene = scene;
	setScene(scene);

	connect(_scene, &drawingboard::CanvasScene::canvasResized, [this](int xoff, int yoff, const QSize &oldsize) {
		if(oldsize.isEmpty()) {
			centerOn(_scene->sceneRect().center());
		} else {
			scrollContentsBy(-xoff, -yoff);
		}
	});

}

void CanvasView::zoomSteps(int steps)
{
	if(_zoom<100 || (_zoom==100 && steps<0))
		setZoom(qRound((_zoom + steps * 10) / 10) * 10);
	else
		setZoom(qRound((_zoom + steps * 50) / 50) * 50);
}

void CanvasView::zoomin()
{
	zoomSteps(1);
}

void CanvasView::zoomout()
{
	zoomSteps(-1);
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

	nm.scale(_mirror ? -1 : 1, _flip ? -1 : 1);

	setMatrix(nm);
	emit viewTransformed(_zoom, _rotate);
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

void CanvasView::setViewFlip(bool flip)
{
	if(flip != _flip) {
		_flip = flip;
		setZoom(_zoom);
	}
}

void CanvasView::setViewMirror(bool mirror)
{
	if(mirror != _mirror) {
		_mirror = mirror;
		setZoom(_zoom);
	}
}

void CanvasView::setLocked(bool lock)
{
	if(lock && !_locked)
		notification::playSound(notification::Event::LOCKED);
	else if(!lock && _locked)
		notification::playSound(notification::Event::UNLOCKED);

	_locked = lock;
	resetCursor();
}

void CanvasView::setToolCursor(const QCursor &cursor)
{
	m_toolcursor = cursor;
	resetCursor();
}

void CanvasView::resetCursor()
{
	if(_locked) {
		viewport()->setCursor(Qt::ForbiddenCursor);

	} else {
		// The standard crosshair cursor is (in some themes) too
		// thick for accurate work, so we use a custom 1px wide cursor.
		// Crosshair is safe to hide, because it is only used with brush
		// tools for which an outline cursor is also drawn.
		if(m_toolcursor.shape() == Qt::CrossCursor) {
			viewport()->setCursor(_enablecrosshair ? _crosshaircursor : QCursor(Qt::BlankCursor));
		} else {
			viewport()->setCursor(m_toolcursor);
		}
	}
}

void CanvasView::setCrosshair(bool enable)
{
	_enablecrosshair = enable;
	resetCursor();
}

void CanvasView::setPixelGrid(bool enable)
{
	_pixelgrid = enable;
	viewport()->update();
}

/**
 * @param radius circle radius
 */
void CanvasView::setOutlineSize(int size)
{
	float updatesize = _outlinesize;
	_outlinesize = size<=0 ? 0 : qMax(2, size);
	if(_showoutline) {
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
	if(_pixelgrid && _zoom >= 800) {
		QPen pen(QColor(160, 160, 160));
		pen.setCosmetic(true);
		painter->setPen(pen);
		for(int x=rect.left();x<=rect.right();++x) {
			painter->drawLine(x, rect.top(), x, rect.bottom()+1);
		}

		for(int y=rect.top();y<=rect.bottom();++y) {
			painter->drawLine(rect.left(), y, rect.right()+1, y);
		}
	}
	if(_showoutline && _outlinesize>0 && !_specialpenmode && !_locked) {
		const QRectF outline(_prevoutlinepoint-QPointF(_outlinesize/2, _outlinesize/2),
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
	_showoutline = true;

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
	_showoutline = false;
	updateOutline();
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
		// TODO
		//_scene->hideUserMarker();
	}
}

void CanvasView::setPressureMapping(const PressureMapping &mapping)
{
	m_pressuremapping = mapping;
}

void CanvasView::onPenDown(const paintcore::Point &p, bool right)
{
	Q_UNUSED(right);
	if(_scene->hasImage() && !_locked) {
		if(_specialpenmode) {
			// quick color pick mode
			_scene->model()->pickColor(p.x(), p.y(), 0, 0);
		} else {
			emit penDown(p, p.pressure());
		}
	}
}

void CanvasView::onPenMove(const paintcore::Point &p, bool right, bool shift, bool alt)
{
	Q_UNUSED(right);

	if(_scene->hasImage() && !_locked) {
		if(_specialpenmode) {
			// quick color pick mode
			_scene->model()->pickColor(p.x(), p.y(), 0, 0);
		} else {
			emit penMove(p, p.pressure(), shift, alt);
		}
	}
}

void CanvasView::onPenUp(bool right)
{
	Q_UNUSED(right);
	if(!_locked) {
		if(!_specialpenmode)
			emit penUp();
	}
	_specialpenmode = false;
}

void CanvasView::penPressEvent(const QPointF &pos, float pressure, Qt::MouseButton button, Qt::KeyboardModifiers modifiers, bool isStylus)
{
	if(button == Qt::MidButton || _dragbtndown) {
		ViewTransform mode;
		if(_dragbtndown == DRAG_NOTRANSFORM) {
			if(modifiers.testFlag(Qt::ControlModifier))
				mode = DRAG_ZOOM;
			else if(modifiers.testFlag(Qt::ShiftModifier))
				mode = DRAG_QUICKADJUST1;
			else
				mode = DRAG_TRANSLATE;
		} else
			mode = _dragbtndown;

		startDrag(pos.x(), pos.y(), mode);

	} else if((button == Qt::LeftButton || button == Qt::RightButton) && _isdragging==DRAG_NOTRANSFORM) {
		_pendown = isStylus ? TABLETDOWN : MOUSEDOWN;
		_pointerdistance = 0;
		_pointervelocity = 0;
		_prevpoint = mapToScene(pos, pressure);
		_specialpenmode = modifiers.testFlag(Qt::ControlModifier);
		onPenDown(mapToScene(pos, mapPressure(pressure, isStylus)), button == Qt::RightButton);
	}
}

//! Handle mouse press events
void CanvasView::mousePressEvent(QMouseEvent *event)
{
	/** @todo why do we sometimes get mouse events for tablet strokes? */
	if(_pendown != NOTDOWN)
		return;

	if(_touching)
		return;

	penPressEvent(
		event->pos(),
		_tabletmode == HYBRID_TABLET && _stylusDown ? _lastPressure : 1,
		event->button(),
		event->modifiers(),
		false
	);
}

void CanvasView::penMoveEvent(const QPointF &pos, float pressure, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, bool isStylus)
{
	if(_isdragging) {
		moveDrag(pos.x(), pos.y());

	} else {
		// Hot border detection. This is used to show the menu bar in fullscreen mode
		// when the pointer is brought to the top of the screen.
		if(!_pendown) {
			if(_hotBorderTop) {
				if(pos.y() > 30) {
					emit hotBorder(false);
					_hotBorderTop = false;
				}
			} else {
				if(pos.y() < 3) {
					emit hotBorder(true);
					_hotBorderTop = true;
				}
			}
		}

		paintcore::Point point = mapToScene(pos, pressure);
		updateOutline(point);
		if(!_prevpoint.intSame(point)) {
			if(_pendown) {
				_pointervelocity = point.distance(_prevpoint);
				_pointerdistance += _pointervelocity;
				point.setPressure(mapPressure(pressure, isStylus));
				onPenMove(point, buttons.testFlag(Qt::RightButton), modifiers.testFlag(Qt::ShiftModifier), modifiers.testFlag(Qt::AltModifier));

			} else if(_pointertracking && _scene->hasImage()) {
				emit pointerMoved(point);
			}
			_prevpoint = point;
		}
	}
}

//! Handle mouse motion events
void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
	/** @todo why do we sometimes get mouse events for tablet strokes? */
	if(_pendown == TABLETDOWN)
		return;
	if(_touching)
		return;
	if(_pendown && event->buttons() == Qt::NoButton) {
		// In case we missed a mouse release
		mouseReleaseEvent(event);
		return;
	}

	penMoveEvent(
		event->pos(),
		_tabletmode == HYBRID_TABLET && _stylusDown ? _lastPressure : 1.0,
		event->buttons(),
		event->modifiers(),
		false
	);
}

void CanvasView::penReleaseEvent(const QPointF &pos, Qt::MouseButton button)
{
	_prevpoint = mapToScene(pos, 0.0);
	if(_isdragging) {
		stopDrag();

	} else if(_pendown == TABLETDOWN || ((button == Qt::LeftButton || button == Qt::RightButton) && _pendown == MOUSEDOWN)) {
		onPenUp(button == Qt::RightButton);
		_pendown = NOTDOWN;
	}
}

//! Handle mouse release events
void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
	if(_touching)
		return;
	penReleaseEvent(event->pos(), event->button());
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
			zoomSteps(steps);
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

		if(_enableTouchPinch && (pinch->changeFlags() & QPinchGesture::ScaleFactorChanged))
			setZoom(_gestureStartZoom * pinch->scaleFactor());

		if(_enableTouchTwist && (pinch->changeFlags() & QPinchGesture::RotationAngleChanged))
			setRotation(_gestureStartAngle + pinch->rotationAngle());
	}
}

static qreal squareDist(const QPointF &p)
{
	return p.x()*p.x() + p.y()*p.y();
}

void CanvasView::setTouchGestures(bool scroll, bool pinch, bool twist)
{
	_enableTouchScroll = scroll;
	_enableTouchPinch = pinch;
	_enableTouchTwist = twist;
}

void CanvasView::touchEvent(QTouchEvent *event)
{
	event->accept();

	switch(event->type()) {
	case QEvent::TouchBegin:
		_touchRotating = false;
		break;

	case QEvent::TouchUpdate: {
		QPointF startCenter, lastCenter, center;
		const int points = event->touchPoints().size();
		for(const auto &tp : event->touchPoints()) {
			startCenter += tp.startPos();
			lastCenter += tp.lastPos();
			center += tp.pos();
		}
		startCenter /= points;
		lastCenter /= points;
		center /= points;

		if(!_touching) {
			_touchStartZoom = zoom();
			_touchStartRotate = rotation();
		}

		// Single finger drag when touch scroll is enabled,
		// but also drag with a pinch gesture. Single finger drag
		// may be deactivated to support finger painting.
		if(_enableTouchScroll || (_enableTouchPinch && points >= 2)) {
			_touching = true;
			float dx = center.x() - lastCenter.x();
			float dy = center.y() - lastCenter.y();
			horizontalScrollBar()->setValue(horizontalScrollBar()->value() - dx);
			verticalScrollBar()->setValue(verticalScrollBar()->value() - dy);
		}

		// Scaling and rotation with two fingers
		if(points >= 2 && (_enableTouchPinch | _enableTouchTwist)) {
			_touching = true;
			float startAvgDist=0, avgDist=0;
			for(const auto &tp : event->touchPoints()) {
				startAvgDist += squareDist(tp.startPos() - startCenter);
				avgDist += squareDist(tp.pos() - center);
			}
			startAvgDist = sqrt(startAvgDist);

			if(_enableTouchPinch) {
				avgDist = sqrt(avgDist);
				const qreal dZoom = avgDist / startAvgDist;
				_zoom = _touchStartZoom * dZoom;
			}

			if(_enableTouchTwist) {
				const QLineF l1 { event->touchPoints().first().startPos(), event->touchPoints().last().startPos() };
				const QLineF l2 { event->touchPoints().first().pos(), event->touchPoints().last().pos() };

				const qreal dAngle = l1.angle() - l2.angle();

				// Require a small nudge to activate rotation to avoid rotating when the user just wanted to zoom
				// Alsom, only rotate when touch points start out far enough from each other. Initial angle measurement
				// is inaccurate when touchpoints are close together.
				if(startAvgDist / _dpi > 0.8 && (qAbs(dAngle) > 3.0 || _touchRotating)) {
					_touchRotating = true;
					_rotate = _touchStartRotate + dAngle;
				}

			}

			// Recalculate view matrix
			setZoom(zoom());
		}

	} break;

	case QEvent::TouchEnd:
	case QEvent::TouchCancel:
		_touching = false;
		break;
	default: break;
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
	}
#ifndef Q_OS_MAC // On Mac, the above gesture events work better
	else if(event->type()==QEvent::TouchBegin || event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel) {
		touchEvent(static_cast<QTouchEvent*>(event));
	}
#endif
	else if(event->type() == QEvent::TabletPress && _tabletmode!=DISABLE_TABLET) {
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		_stylusDown = true;
		_lastPressure = tabev->pressure();

		// Note: it is possible to get a mouse press event for a tablet event (even before
		// the tablet event is received or even though tabev->accept() is called), but
		// it is never possible to get a TabletPress for a real mouse press. Therefore,
		// we don't actually do anything yet in the penDown handler other than remember
		// the initial point and we'll let a TabletEvent override the mouse event.
		if(_tabletmode==ENABLE_TABLET) {
			tabev->accept();

			penPressEvent(
				tabev->posF(),
				tabev->pressure(),
				tabev->button(),
				tabev->modifiers(),
				true
			);
		} else
			return QGraphicsView::viewportEvent(event);
	}
	else if(event->type() == QEvent::TabletMove && _tabletmode!=DISABLE_TABLET) {
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		_lastPressure = tabev->pressure();
		_stylusDown = true;
		if(_tabletmode==ENABLE_TABLET) {
			tabev->accept();

			penMoveEvent(
				tabev->posF(),
				tabev->pressure(),
				tabev->buttons(),
				tabev->modifiers(),
				true
			);
		} else
			return QGraphicsView::viewportEvent(event);
	}
	else if(event->type() == QEvent::TabletRelease && _tabletmode!=DISABLE_TABLET) {
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		_stylusDown = false;
		if(_tabletmode==ENABLE_TABLET) {
			tabev->accept();
			penReleaseEvent(tabev->posF(), tabev->button());

		} else
			return QGraphicsView::viewportEvent(event);
	}
	else {
		return QGraphicsView::viewportEvent(event);
	}

	return true;
}

float CanvasView::mapPressure(float pressure, bool stylus)
{
	switch(m_pressuremapping.mode) {
	case PressureMapping::STYLUS:
		return stylus || (_tabletmode==HYBRID_TABLET && _stylusDown) ? m_pressuremapping.curve.value(pressure) : 1.0;

	case PressureMapping::DISTANCE: {
		qreal d = qMin(_pointerdistance, m_pressuremapping.param) / m_pressuremapping.param;
		return m_pressuremapping.curve.value(d);
	}

	case PressureMapping::VELOCITY:
		qreal v = qMin(_pointervelocity, m_pressuremapping.param) / m_pressuremapping.param;
		return m_pressuremapping.curve.value(v);
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
	if(_showoutline && !_locked && !point.roughlySame(_prevoutlinepoint)) {
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
		emit quickAdjust(delta);
}

QPoint CanvasView::viewCenterPoint() const
{
	return mapToScene(rect().center()).toPoint();
}

bool CanvasView::isPointVisible(const QPointF &point) const
{
	QPoint p = mapFromScene(point);
	return p.x() > 0 && p.y() > 0 && p.x() < width() && p.y() < height();
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
	_showoutline = false;
	updateOutline();
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

void CanvasView::showEvent(QShowEvent *event)
{
	QGraphicsView::showEvent(event);
	// Find the DPI of the screen
	// TODO: if the window is moved to another screen, this should be updated
	QWidget *w = this;
	while(w) {
		if(w->windowHandle() != nullptr) {
			_dpi = w->windowHandle()->screen()->physicalDotsPerInch();
			break;
		}
		w=w->parentWidget();
	}
}

}
