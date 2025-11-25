// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/event_log.h>
}
#include "desktop/main.h"
#include "desktop/view/widgettouchhandler.h"
#include "libclient/config/config.h"
#include <QDateTime>
#include <QGestureEvent>
#include <QTimer>

namespace view {

WidgetTouchHandler::WidgetTouchHandler(QObject *parent)
	: TouchHandler(parent)
{
	config::Config *cfg = dpAppConfig();
	CFG_BIND_SET(
		cfg, TouchDrawPressure, this,
		WidgetTouchHandler::setTouchDrawPressureEnabled);
	CFG_BIND_SET(
		cfg, OneFingerTouch, this, WidgetTouchHandler::setOneFingerTouchAction);
	CFG_BIND_SET(
		cfg, TwoFingerPinch, this, WidgetTouchHandler::setTwoFingerPinchAction);
	CFG_BIND_SET(
		cfg, TwoFingerTwist, this, WidgetTouchHandler::setTwoFingerTwistAction);
	CFG_BIND_SET(
		cfg, OneFingerTap, this, WidgetTouchHandler::setOneFingerTapAction);
	CFG_BIND_SET(
		cfg, TwoFingerTap, this, WidgetTouchHandler::setTwoFingerTapAction);
	CFG_BIND_SET(
		cfg, ThreeFingerTap, this, WidgetTouchHandler::setThreeFingerTapAction);
	CFG_BIND_SET(
		cfg, FourFingerTap, this, WidgetTouchHandler::setFourFingerTapAction);
	CFG_BIND_SET(
		cfg, OneFingerTapAndHold, this,
		WidgetTouchHandler::setOneFingerTapAndHoldAction);
	CFG_BIND_SET(cfg, TouchSmoothing, this, WidgetTouchHandler::setSmoothing);
}

void WidgetTouchHandler::handleGesture(
	QGestureEvent *event, qreal zoom, qreal rotation)
{
	const QTapGesture *tap =
		static_cast<const QTapGesture *>(event->gesture(Qt::TapGesture));
	if(tap) {
		Qt::GestureState tapState = tap->state();
		DP_EVENT_LOG(
			"tap state=0x%x touching=%d", unsigned(tapState),
			int(isTouching()));
		if(tapState == Qt::GestureFinished) {
			emitOneFingerTapAction();
		}
	}

	const QPinchGesture *pinch =
		static_cast<const QPinchGesture *>(event->gesture(Qt::PinchGesture));
	bool hadPinchUpdate = false;
	if(pinch) {
		Qt::GestureState pinchState = pinch->state();
		QPinchGesture::ChangeFlags cf = pinch->changeFlags();
		DP_EVENT_LOG(
			"pinch state=0x%x change=0x%x scale=%f rotation=%f touching=%d",
			unsigned(pinchState), unsigned(cf), pinch->totalScaleFactor(),
			pinch->totalRotationAngle(), int(isTouching()));

		switch(pinchState) {
		case Qt::GestureStarted:
			m_gestureStartZoom = zoom;
			m_gestureStartRotation = rotation;
			startZoomRotate(zoom, rotation);
			Q_FALLTHROUGH();
		case Qt::GestureUpdated:
		case Qt::GestureFinished: {
			if(isTouchDrawOrPanEnabled() &&
			   cf.testFlag(QPinchGesture::CenterPointChanged)) {
				QPointF d = pinch->centerPoint() - pinch->lastCenterPoint();
				scrollBy(-d.x(), -d.y());
			}

			bool haveZoom = isTouchPinchEnabled() &&
							cf.testFlag(QPinchGesture::ScaleFactorChanged);
			bool haveRotation =
				isTouchTwistEnabled() &&
				cf.testFlag(QPinchGesture::RotationAngleChanged);
			if(haveZoom || haveRotation) {
				qreal gestureZoom =
					haveZoom ? m_gestureStartZoom * pinch->totalScaleFactor()
							 : m_gestureStartZoom;
				qreal gestureRotation = haveRotation
											? adjustTwistRotation(
												  m_gestureStartRotation +
												  pinch->totalRotationAngle())
											: m_gestureStartRotation;
				zoomRotate(gestureZoom, gestureRotation);
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
			delta.x(), delta.y(), int(isTouching()));

		switch(panState) {
		case Qt::GestureStarted:
			m_gestureStartZoom = zoom;
			m_gestureStartRotation = rotation;
			startZoomRotate(zoom, rotation);
			Q_FALLTHROUGH();
		case Qt::GestureUpdated:
		case Qt::GestureFinished:
			if(!hadPinchUpdate && isTouchDrawOrPanEnabled()) {
				scrollBy(delta.x() / -2.0, delta.y() / -2.0);
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

}
