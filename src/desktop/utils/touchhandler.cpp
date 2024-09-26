// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/event_log.h>
}
#include "desktop/main.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/utils/touchhandler.h"
#include "libshared/util/qtcompat.h"
#include <QDateTime>
#include <QGestureEvent>

TouchHandler::TouchHandler(QObject *parent)
	: QObject(parent)
	, m_oneFingerTouchAction(int(ONE_FINGER_TOUCH_DEFAULT))
{
	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindOneFingerTouch(this, &TouchHandler::setOneFingerTouchAction);
	settings.bindTwoFingerZoom(this, &TouchHandler::setEnableTouchPinch);
	settings.bindTwoFingerRotate(this, &TouchHandler::setEnableTouchTwist);
}

bool TouchHandler::isTouchDrawEnabled() const
{
	switch(m_oneFingerTouchAction) {
	case int(desktop::settings::OneFingerTouchAction::Draw):
		return true;
	case int(desktop::settings::OneFingerTouchAction::Guess):
		return !m_anyTabletEventsReceived;
	default:
		return false;
	}
}

bool TouchHandler::isTouchPanEnabled() const
{
	switch(m_oneFingerTouchAction) {
	case int(desktop::settings::OneFingerTouchAction::Pan):
		return false;
	case int(desktop::settings::OneFingerTouchAction::Guess):
		return m_anyTabletEventsReceived;
	default:
		return false;
	}
}

bool TouchHandler::isTouchDrawOrPanEnabled() const
{
	switch(m_oneFingerTouchAction) {
	case int(desktop::settings::OneFingerTouchAction::Pan):
	case int(desktop::settings::OneFingerTouchAction::Draw):
	case int(desktop::settings::OneFingerTouchAction::Guess):
		return true;
	default:
		return false;
	}
}

void TouchHandler::handleTouchBegin(QTouchEvent *event)
{
	const QList<compat::TouchPoint> &points = compat::touchPoints(*event);
	int pointsCount = points.size();
	QPointF posf = compat::touchPos(points.first());

	m_touchDrawBuffer.clear();
	m_touchRotating = false;
	if(isTouchDrawEnabled() && pointsCount == 1 && !compat::isTouchPad(event)) {
		DP_EVENT_LOG(
			"touch_draw_begin x=%f y=%f touching=%d type=%d device=%s "
			"points=%s timestamp=%llu",
			posf.x(), posf.y(), int(m_touching), compat::touchDeviceType(event),
			qUtf8Printable(compat::touchDeviceName(event)),
			qUtf8Printable(compat::debug(points)),
			qulonglong(event->timestamp()));
		if(isTouchPanEnabled() || m_enableTouchPinch || m_enableTouchTwist) {
			// Buffer the touch first, since it might end up being the
			// beginning of an action that involves multiple fingers.
			m_touchDrawBuffer.append(
				{QDateTime::currentMSecsSinceEpoch(), posf});
			m_touchMode = TouchMode::Unknown;
		} else {
			// There's no other actions other than drawing enabled, so we
			// can just start drawing without awaiting what happens next.
			m_touchMode = TouchMode::Drawing;
			emit touchPressed(event, QDateTime::currentMSecsSinceEpoch(), posf);
		}
	} else {
		DP_EVENT_LOG(
			"touch_begin touching=%d type=%d device=%s points=%s "
			"timestamp=%llu",
			int(m_touching), compat::touchDeviceType(event),
			qUtf8Printable(compat::touchDeviceName(event)),
			qUtf8Printable(compat::debug(points)),
			qulonglong(event->timestamp()));
		m_touchMode = TouchMode::Moving;
	}
}

void TouchHandler::handleTouchUpdate(
	QTouchEvent *event, qreal zoom, qreal rotation, qreal dpr)
{
	const QList<compat::TouchPoint> &points = compat::touchPoints(*event);
	int pointsCount = points.size();

	if(isTouchDrawEnabled() &&
	   ((pointsCount == 1 && m_touchMode == TouchMode::Unknown) ||
		m_touchMode == TouchMode::Drawing) &&
	   !compat::isTouchPad(event)) {
		QPointF posf = compat::touchPos(compat::touchPoints(*event).first());
		DP_EVENT_LOG(
			"touch_draw_update x=%f y=%f touching=%d type=%d device=%s "
			"points=%s timestamp=%llu",
			posf.x(), posf.y(), int(m_touching), compat::touchDeviceType(event),
			qUtf8Printable(compat::touchDeviceName(event)),
			qUtf8Printable(compat::debug(points)),
			qulonglong(event->timestamp()));
		int bufferCount = m_touchDrawBuffer.size();
		if(bufferCount == 0) {
			if(m_touchMode == TouchMode::Drawing) {
				emit touchMoved(QDateTime::currentMSecsSinceEpoch(), posf);
			} else { // Shouldn't happen, but we'll deal with it anyway.
				m_touchMode = TouchMode::Drawing;
				emit touchPressed(
					event, QDateTime::currentMSecsSinceEpoch(), posf);
			}
		} else {
			// This still might be the beginning of a multitouch operation.
			// If the finger didn't move enough of a distance and we didn't
			// buffer an excessive amount of touches yet. Buffer the touched
			// point and wait a bit more as to what's going to happen.
			bool shouldAppend =
				bufferCount < TOUCH_DRAW_BUFFER_COUNT &&
				QLineF(m_touchDrawBuffer.first().second, posf).length() <
					TOUCH_DRAW_DISTANCE;
			if(shouldAppend) {
				m_touchDrawBuffer.append(
					{QDateTime::currentMSecsSinceEpoch(), posf});
			} else {
				m_touchMode = TouchMode::Drawing;
				flushTouchDrawBuffer();
				emit touchMoved(QDateTime::currentMSecsSinceEpoch(), posf);
			}
		}
	} else {
		m_touchMode = TouchMode::Moving;

		QPointF startCenter, lastCenter, center;
		for(const auto &tp : compat::touchPoints(*event)) {
			startCenter += compat::touchStartPos(tp);
			lastCenter += compat::touchLastPos(tp);
			center += compat::touchPos(tp);
		}
		startCenter /= pointsCount;
		lastCenter /= pointsCount;
		center /= pointsCount;

		DP_EVENT_LOG(
			"touch_update x=%f y=%f touching=%d type=%d device=%s points=%s "
			"timestamp=%llu",
			center.x(), center.y(), int(m_touching),
			compat::touchDeviceType(event),
			qUtf8Printable(compat::touchDeviceName(event)),
			qUtf8Printable(compat::debug(points)),
			qulonglong(event->timestamp()));

		if(!m_touching) {
			m_touchStartZoom = zoom;
			m_touchStartRotate = rotation;
		}

		// We want to pan with one finger if one-finger pan is enabled and
		// also when pinching to zoom. Slightly non-obviously, we also want
		// to pan with one finger when finger drawing is enabled, because if
		// we got here with one finger, we've come out of a multitouch
		// operation and aren't going to be drawing until all fingers leave
		// the surface anyway, so panning is the only sensible option.
		bool haveMultiTouch = pointsCount >= 2;
		bool havePinchOrTwist =
			haveMultiTouch && (m_enableTouchPinch || m_enableTouchTwist);
		bool havePan = havePinchOrTwist ||
					   (isTouchDrawOrPanEnabled() &&
						(haveMultiTouch || !compat::isTouchPad(event)));
		if(havePan) {
			m_touching = true;
			qreal dx = center.x() - lastCenter.x();
			qreal dy = center.y() - lastCenter.y();
			emit touchScrolledBy(-dx, -dy);
		}

		// Scaling and rotation with two fingers
		if(havePinchOrTwist) {
			m_touching = true;
			qreal startAvgDist = 0.0;
			qreal avgDist = 0.0;
			for(const compat::TouchPoint &tp : compat::touchPoints(*event)) {
				startAvgDist +=
					squareDist(compat::touchStartPos(tp) - startCenter);
				avgDist += squareDist(compat::touchPos(tp) - center);
			}
			startAvgDist = sqrt(startAvgDist);

			qreal touchZoom = zoom;
			if(m_enableTouchPinch) {
				avgDist = sqrt(avgDist);
				qreal dZoom = avgDist / startAvgDist;
				touchZoom = m_touchStartZoom * dZoom;
			}

			qreal touchRotation = rotation;
			if(m_enableTouchTwist) {
				QLineF l1(
					compat::touchStartPos(points.first()),
					compat::touchStartPos(points.last()));
				QLineF l2(
					compat::touchPos(points.first()),
					compat::touchPos(points.last()));
				qreal dAngle = l1.angle() - l2.angle();

				// Require a small nudge to activate rotation to avoid
				// rotating when the user just wanted to zoom. Also only
				// rotate when touch points start out far enough from each
				// other. Initial angle measurement is inaccurate when
				// touchpoints are close together.
				if(startAvgDist * dpr > 80.0 &&
				   (qAbs(dAngle) > 3.0 || m_touchRotating)) {
					m_touchRotating = true;
					touchRotation = m_touchStartRotate + dAngle;
				}
			}

			emit touchZoomedRotated(touchZoom, touchRotation);
		}
	}
}

void TouchHandler::handleTouchEnd(QTouchEvent *event, bool cancel)
{
	event->accept();
	const QList<compat::TouchPoint> &points = compat::touchPoints(*event);
	if(isTouchDrawEnabled() &&
	   ((m_touchMode == TouchMode::Unknown && !m_touchDrawBuffer.isEmpty()) ||
		m_touchMode == TouchMode::Drawing)) {
		DP_EVENT_LOG(
			"touch_draw_%s touching=%d type=%d device=%s points=%s "
			"timestamp=%llu",
			cancel ? "cancel" : "end", int(m_touching),
			compat::touchDeviceType(event),
			qUtf8Printable(compat::touchDeviceName(event)),
			qUtf8Printable(compat::debug(points)),
			qulonglong(event->timestamp()));
		flushTouchDrawBuffer();
		emit touchReleased(
			QDateTime::currentMSecsSinceEpoch(),
			compat::touchPos(compat::touchPoints(*event).first()));
	} else {
		DP_EVENT_LOG(
			"touch_%s touching=%d type=%d device=%s points=%s timestamp=%llu",
			cancel ? "cancel" : "end", int(m_touching),
			compat::touchDeviceType(event),
			qUtf8Printable(compat::touchDeviceName(event)),
			qUtf8Printable(compat::debug(points)),
			qulonglong(event->timestamp()));
	}
	m_touching = false;
}

void TouchHandler::handleGesture(
	QGestureEvent *event, qreal zoom, qreal rotation)
{
	const QPinchGesture *pinch =
		static_cast<const QPinchGesture *>(event->gesture(Qt::PinchGesture));
	bool hadPinchUpdate = false;
	if(pinch) {
		Qt::GestureState pinchState = pinch->state();
		QPinchGesture::ChangeFlags cf = pinch->changeFlags();
		DP_EVENT_LOG(
			"pinch state=0x%x change=0x%x scale=%f rotation=%f touching=%d",
			unsigned(pinchState), unsigned(cf), pinch->totalScaleFactor(),
			pinch->totalRotationAngle(), int(m_touching));

		switch(pinchState) {
		case Qt::GestureStarted:
			m_gestureStartZoom = zoom;
			m_gestureStartRotation = rotation;
			Q_FALLTHROUGH();
		case Qt::GestureUpdated:
		case Qt::GestureFinished: {
			if(isTouchDrawOrPanEnabled() &&
			   cf.testFlag(QPinchGesture::CenterPointChanged)) {
				QPointF d = pinch->centerPoint() - pinch->lastCenterPoint();
				emit touchScrolledBy(-d.x(), -d.y());
			}

			bool haveZoom = m_enableTouchPinch &&
							cf.testFlag(QPinchGesture::ScaleFactorChanged);
			bool haveRotation =
				m_enableTouchTwist &&
				cf.testFlag(QPinchGesture::RotationAngleChanged);
			if(haveZoom || haveRotation) {
				qreal gestureZoom =
					haveZoom ? m_gestureStartZoom * pinch->totalScaleFactor()
							 : m_gestureStartZoom;
				qreal gestureRotation =
					haveRotation
						? m_gestureStartRotation + pinch->totalRotationAngle()
						: m_gestureStartRotation;
				emit touchZoomedRotated(gestureZoom, gestureRotation);
			}

			hadPinchUpdate = true;
			Q_FALLTHROUGH();
		}
		case Qt::GestureCanceled:
			event->accept();
			break;
		default:
			break;
		}
	}

	const QPanGesture *pan =
		static_cast<const QPanGesture *>(event->gesture(Qt::PanGesture));
	if(pan) {
		Qt::GestureState panState = pan->state();
		QPointF delta = pan->delta();
		DP_EVENT_LOG(
			"pan state=0x%x dx=%f dy=%f touching=%d", unsigned(panState),
			delta.x(), delta.y(), int(m_touching));

		switch(panState) {
		case Qt::GestureStarted:
			m_gestureStartZoom = zoom;
			m_gestureStartRotation = rotation;
			Q_FALLTHROUGH();
		case Qt::GestureUpdated:
		case Qt::GestureFinished:
			if(!hadPinchUpdate && isTouchDrawOrPanEnabled()) {
				emit touchScrolledBy(delta.x() / -2.0, delta.y() / -2.0);
			}
			Q_FALLTHROUGH();
		case Qt::GestureCanceled:
			event->accept();
			break;
		default:
			break;
		}
	}
}

void TouchHandler::setOneFingerTouchAction(int oneFingerTouchAction)
{
	switch(oneFingerTouchAction) {
	case int(desktop::settings::OneFingerTouchAction::Nothing):
	case int(desktop::settings::OneFingerTouchAction::Draw):
	case int(desktop::settings::OneFingerTouchAction::Pan):
	case int(desktop::settings::OneFingerTouchAction::Guess):
		m_oneFingerTouchAction = oneFingerTouchAction;
		break;
	default:
		qWarning("Unknown one finger touch action %d", oneFingerTouchAction);
		break;
	}
}

void TouchHandler::setEnableTouchPinch(bool enableTouchPinch)
{
	m_enableTouchPinch = enableTouchPinch;
}

void TouchHandler::setEnableTouchTwist(bool enableTouchTwist)
{
	m_enableTouchTwist = enableTouchTwist;
}

void TouchHandler::flushTouchDrawBuffer()
{
	int bufferCount = m_touchDrawBuffer.size();
	if(bufferCount != 0) {
		const QPair<long long, QPointF> &press = m_touchDrawBuffer.first();
		emit touchPressed(nullptr, press.first, press.second);
		for(int i = 0; i < bufferCount; ++i) {
			const QPair<long long, QPointF> &move = m_touchDrawBuffer[i];
			emit touchMoved(move.first, move.second);
		}
		m_touchDrawBuffer.clear();
	}
}
