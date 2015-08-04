/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include "layerstackitem.h"
#include "canvasinputarea.h"

#include <QtMath>

CanvasInputArea::CanvasInputArea(QQuickItem *parent)
	: QQuickItem(parent),
	m_canvas(nullptr), m_tabletstate(nullptr),

	m_enableTouchScroll(true), m_enableTouchPinch(true), m_enableTouchRotate(true),

	m_colorPickerCursor(QCursor(QPixmap(":/cursors/colorpicker.png"), 2, 29)),
	m_crosshairCursor(QCursor(QPixmap(":/cursors/crosshair.png"), 15, 15)),

	m_locked(false),

	m_contentx(0), m_contenty(0),
	m_rotation(0), m_zoom(1),

	m_specialpenmode(false),
	m_isdragging(DRAG_NOTRANSFORM),
	m_dragbtndown(DRAG_NOTRANSFORM),
	m_dragx(0), m_dragy(0),

	m_pointerdistance(0), m_pointervelocity(0),
	m_prevshift(false), m_prevalt(false),

	m_zoomWheelDelta(0),

	m_pendown(false), m_touching(false)
{
	setAcceptedMouseButtons(Qt::AllButtons);
	setAcceptHoverEvents(true);
	setFlag(ItemIsFocusScope);	

	m_toolcursor = QCursor(Qt::CrossCursor);
	resetCursor();
}

QPointF CanvasInputArea::mapToCanvas(const QPointF &point) const
{
	return mapToItem(m_canvas, point);
}

void CanvasInputArea::setContentZoom(qreal zoom) {
	if(zoom <= 0 || zoom == m_zoom)
		return;
	m_zoom = zoom;
	emit contentZoomChanged(zoom);
}

void CanvasInputArea::setLocked(bool lock)
{
	if(m_locked != lock) {
		m_locked = lock;
		resetCursor();
		emit isLockedChanged(lock);
	}
}

void CanvasInputArea::setToolCursor(const QCursor &cursor)
{
	m_toolcursor = cursor;
	resetCursor();
	emit toolCursorChanged();
}

void CanvasInputArea::resetCursor()
{
	if(m_locked) {
		setCursor(QCursor(Qt::ForbiddenCursor));

	} else {
		// The standard crosshair cursor is (in some themes) too
		// thick for accurate work, so we use a custom 1px wide cursor.
		// Crosshair is safe to hide, because it is only used with brush
		// tools for which an outline cursor is also drawn.
		if(m_toolcursor.shape() == Qt::CrossCursor) {
			// TODO
			//setCursor(_enablecrosshair ? _crosshaircursor : QCursor(Qt::BlankCursor));
			setCursor(m_crosshairCursor);
		} else {
			setCursor(m_toolcursor);
		}
	}
}

void CanvasInputArea::penPressEvent(const QPointF &pos, float pressure, Qt::MouseButton button, Qt::KeyboardModifiers modifiers, bool isStylus)
{
	//qDebug() << "penPress" << mapToCanvas(pos) << pressure << button << modifiers << isStylus;

	if(button == Qt::MidButton || m_dragbtndown) {
		ViewTransform mode;
		if(m_dragbtndown == DRAG_NOTRANSFORM) {
			if(modifiers.testFlag(Qt::ControlModifier))
				mode = DRAG_ZOOM;
			else if(modifiers.testFlag(Qt::ShiftModifier))
				mode = DRAG_QUICKADJUST1;
			else
				mode = DRAG_TRANSLATE;
		} else
			mode = m_dragbtndown;

		startDrag(pos, mode);

	} else if((button == Qt::LeftButton || button == Qt::RightButton) && m_isdragging==DRAG_NOTRANSFORM) {
		m_pendown = true;
		m_pointerdistance = 0;
		m_pointervelocity = 0;
		m_specialpenmode = modifiers.testFlag(Qt::ControlModifier);

		const QPointF point = mapToCanvas(pos);
		m_prevpoint = point;

		if(m_specialpenmode) {
			emit colorPickRequested(pos);
		} else {
			emit penDown(point, mapPressure(pressure, isStylus));
		}
		// TODO right click signal
	}

}

void CanvasInputArea::penMoveEvent(const QPointF &pos, float pressure, Qt::KeyboardModifiers modifiers, bool isStylus)
{
	//qDebug() << "penMove" << mapToCanvas(pos) << pressure << modifiers << isStylus;

	if(m_isdragging) {
		moveDrag(pos);

	} else {
		const QPointF point = mapToCanvas(pos);

		//updateOutline(point);
		if(!paintcore::Point::intSame(m_prevpoint, point)) {
			if(m_pendown) {
				m_pointervelocity = paintcore::Point::distance(m_prevpoint, point);
				m_pointerdistance += m_pointervelocity;

				if(m_specialpenmode)
					emit colorPickRequested(pos);
				else
					emit penMove(point, mapPressure(pressure, isStylus), modifiers.testFlag(Qt::ShiftModifier), modifiers.testFlag(Qt::AltModifier));
			}
#if 0 // TODO
			else if(m_pointertracking && _scene->hasImage()) {
				emit pointerMoved(point);
			}
#endif
			m_prevpoint = point;
		}
	}
}

void CanvasInputArea::penReleaseEvent(const QPointF &pos, Qt::MouseButton button)
{
	qDebug() << "penRelease" << mapToCanvas(pos) << button;
	m_prevpoint = mapToCanvas(pos, 0.0);
	if(m_isdragging) {
		stopDrag();

	} else {
		m_pendown = false;
		if(!m_specialpenmode)
			emit penUp();
	}

	m_specialpenmode = false;
}

qreal CanvasInputArea::mapPressure(qreal pressure, bool stylus)
{
	switch(m_pressuremapping.mode) {
	case PressureMapping::STYLUS:
		return stylus ? m_pressuremapping.curve.value(pressure) : 1.0;

	case PressureMapping::DISTANCE: {
		qreal d = qMin(m_pointerdistance, m_pressuremapping.param) / m_pressuremapping.param;
		return m_pressuremapping.curve.value(d);
		}

	case PressureMapping::VELOCITY: {
		qreal v = qMin(m_pointervelocity, m_pressuremapping.param) / m_pressuremapping.param;
		return m_pressuremapping.curve.value(v);
		}
	}

	// Shouldn't be reached
	Q_ASSERT(false);
	return 0;
}

void CanvasInputArea::startDrag(const QPointF &pos, ViewTransform mode)
{
	setCursor(Qt::ClosedHandCursor);
	m_dragx = pos.x();
	m_dragy = pos.y();
	m_isdragging = mode;
#if 0
	_showoutline = false;
	updateOutline();
#endif
}

void CanvasInputArea::moveDrag(const QPointF &pos)
{
	const qreal dx = m_dragx - pos.x();
	const qreal dy = m_dragy - pos.y();

	if(m_isdragging==DRAG_ROTATE) {
		qreal preva = qAtan2( width()/2 - m_dragx, height()/2 - m_dragy );
		qreal a = qAtan2( width()/2 - pos.x(), height()/2 - pos.y() );
		setContentRotation(contentRotation() + qRadiansToDegrees(preva-a));

	} else if(m_isdragging==DRAG_ZOOM) {
		if(dy!=0) {
			float delta = qBound(-1.0, dy / 100.0, 1.0);
			if(delta>0) {
				setContentZoom(contentZoom() * (1+delta));
			} else if(delta<0) {
				setContentZoom(contentZoom() / (1-delta));
			}
		}
	} else if(m_isdragging==DRAG_QUICKADJUST1) {
		if(dy!=0) {
			qreal delta = qBound(-2.0, dy / 10.0, 2.0);

			emit quickAdjust(delta);
		}
	} else {
		setContentX(contentX() - dx / contentZoom());
		setContentY(contentY() - dy / contentZoom());
	}

	m_dragx = pos.x();
	m_dragy = pos.y();
}

void CanvasInputArea::stopDrag()
{
	if(m_dragbtndown != DRAG_NOTRANSFORM)
		setCursor(Qt::OpenHandCursor);
	else
		resetCursor();

	m_isdragging = DRAG_NOTRANSFORM;
	// TODO
	//m_showoutline = true;
}

void CanvasInputArea::mousePressEvent(QMouseEvent *event)
{
	if(m_touching)
		return;

	const bool stylusDown = m_tabletstate ? m_tabletstate->isStylusDown() : false;

	penPressEvent(
		event->pos(),
		stylusDown ? m_tabletstate->isStylusDown() : 1,
		event->button(),
		event->modifiers(),
		false
	);
}

void CanvasInputArea::hoverMoveEvent(QHoverEvent *event)
{
	if(m_pendown) {
		// In case we missed a mouse release
		QMouseEvent me(QEvent::MouseButtonRelease, event->pos(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
		mouseReleaseEvent(&me);
		return;
	}

	penMoveEvent(
		event->pos(),
		0,
		Qt::NoModifier,
		false
	);
}

void CanvasInputArea::mouseMoveEvent(QMouseEvent *event)
{
	if(m_touching)
		return;
	if(m_pendown && event->buttons() == Qt::NoButton) {
		// In case we missed a mouse release
		mouseReleaseEvent(event);
		return;
	}

	const bool stylusDown = m_tabletstate ? m_tabletstate->isStylusDown() : false;

	penMoveEvent(
		event->pos(),
		stylusDown ? m_tabletstate->lastPressure() : 1.0,
		event->modifiers(),
		stylusDown
	);
}

void CanvasInputArea::mouseReleaseEvent(QMouseEvent *event)
{
	if(m_touching)
		return;
	penReleaseEvent(event->pos(), event->button());
}

void CanvasInputArea::zoomSteps(int steps)
{
	const qreal z = contentZoom() * 100;

	if(z<100 || (z==100 && steps<0))
		setContentZoom(qRound((z + steps * 10) / 10) * 10 / 100.0);
	else
		setContentZoom(qRound((z + steps * 50) / 50) * 50 / 100.0);
}

void CanvasInputArea::wheelEvent(QWheelEvent *event)
{
	if((event->modifiers() & Qt::ControlModifier)) {
		m_zoomWheelDelta += event->angleDelta().y();

		const int steps = m_zoomWheelDelta / 120;
		m_zoomWheelDelta -= steps * 120;

		if(steps != 0) {
			zoomSteps(steps);
		}

	} else {
		QPoint d = event->pixelDelta();
		if(d.isNull())
			d = QPoint(
				event->angleDelta().x() / 8,
				event->angleDelta().y() / 8
			);

		if((event->modifiers() & Qt::ShiftModifier)) {
			emit quickAdjust(d.y());

		} else {
			setContentX(contentX() + d.x());
			setContentY(contentY() + d.y());
		}
	}
}

void CanvasInputArea::hoverEnterEvent(QHoverEvent *event)
{
	QQuickItem::hoverEnterEvent(event);
#if 0
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
		forceActiveFocus();
	}
#endif

	forceActiveFocus();
}

void CanvasInputArea::nativeGestureEvent(QNativeGestureEvent *event)
{
	switch(event->gestureType()) {
#if 0
	case Qt::BeginNativeGesture:
	case Qt::EndNativeGesture:
		break;
#endif
	case Qt::ZoomNativeGesture: {
		const qreal z = contentZoom() + event->value();
		if(event->value() > 0 || z > 0.25)
			setContentZoom(z);
		break;
		}
	case Qt::RotateNativeGesture:
		setContentRotation(contentRotation() + event->value());
		break;
	default: break;
	}
}

void CanvasInputArea::keyPressEvent(QKeyEvent *event) {
	qDebug() << "keypress" << event->key();
	if(event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
		event->accept();
		if(event->modifiers() & Qt::ControlModifier) {
			m_dragbtndown = DRAG_ROTATE;
		} else {
			m_dragbtndown = DRAG_TRANSLATE;
		}
		setCursor(Qt::OpenHandCursor);

	} else {
		QQuickItem::keyPressEvent(event);

		if(event->key() == Qt::Key_Control && !m_dragbtndown)
			setCursor(m_colorPickerCursor);
	}
}

void CanvasInputArea::keyReleaseEvent(QKeyEvent *event) {
	qDebug() << "keyrelease" << event->key();
	if(event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
		event->accept();
		m_dragbtndown = DRAG_NOTRANSFORM;

		if(m_isdragging==DRAG_NOTRANSFORM)
			resetCursor();

	} else {
		QQuickItem::keyReleaseEvent(event);

		if(event->key() == Qt::Key_Control) {
			if(m_dragbtndown)
				setCursor(Qt::OpenHandCursor);
			else
				resetCursor();
		}
	}
}

bool CanvasInputArea::event(QEvent *event)
{
	switch(event->type()) {
	case QEvent::NativeGesture:
		nativeGestureEvent(static_cast<QNativeGestureEvent*>(event));
		return true;
	default: return QQuickItem::event(event);
	}
}
