// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/globalkeyeventfilter.h"
#include "desktop/utils/qtguicompat.h"
#include <QApplication>
#include <QDateTime>
#include <QKeyEvent>

GlobalKeyEventFilter::GlobalKeyEventFilter(QObject *parent)
	: QObject{parent}
	, m_lastAltPress{0}
	, m_lastAltInternalTimestamp{0}
{
}

bool GlobalKeyEventFilter::eventFilter(QObject *watched, QEvent *event)
{
	switch(event->type()) {
	case QEvent::KeyRelease: {
		QKeyEvent *ke = static_cast<QKeyEvent *>(event);
		checkCanvasFocus(ke);
		break;
	}
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_ANDROID)
	case QEvent::TabletEnterProximity: {
		onTabletEventReceived();
		m_anyTabletProximityEventsReceived = true;
		QTabletEvent *te = static_cast<QTabletEvent *>(event);
		bool eraser = compat::isEraser(*te);
		emit tabletProximityChanged(true, eraser);
		updateEraserNear(eraser);
		return true;
	}
	case QEvent::TabletLeaveProximity: {
		onTabletEventReceived();
		m_anyTabletProximityEventsReceived = true;
		QTabletEvent *te = static_cast<QTabletEvent *>(event);
		bool eraser = compat::isEraser(*te);
		emit tabletProximityChanged(false, eraser);
		if(eraser) {
			updateEraserNear(false);
		}
		return true;
	}
#endif
	case QEvent::TabletPress:
		onTabletEventReceived();
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_ANDROID)
		m_tabletDown = true;
#endif
		break;
	case QEvent::TabletMove:
		onTabletEventReceived();
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_ANDROID)
		// Workaround for tablets that don't send proximity events. Only do this
		// if no proximity events have been received (to avoid conflicting
		// information), the tablet is not pressed down (to evade tablets that
		// don't give the necessary information when hovering) and we actually
		// saw both a pen and an eraser tip at some point (to avoid being
		// perpetually stuck erasing.)
		if(!m_anyTabletProximityEventsReceived && !m_tabletDown) {
			QTabletEvent *te = static_cast<QTabletEvent *>(event);
			bool eraser = compat::isEraser(*te);

			if(eraser) {
				m_sawEraserTip = true;
			} else {
				m_sawPenTip = true;
			}

			if(m_sawPenTip && m_sawEraserTip) {
				updateEraserNear(eraser);
			}
		}
#endif
		break;
	case QEvent::TabletRelease:
		onTabletEventReceived();
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_ANDROID)
		m_tabletDown = false;
#endif
		break;
	default:
		break;
	}
	return QObject::eventFilter(watched, event);
}

void GlobalKeyEventFilter::checkCanvasFocus(QKeyEvent *event)
{
	bool altReleased = compat::keyPressed(*event) ==
					   compat::keyCombination(Qt::NoModifier, Qt::Key_Alt);
	if(altReleased) {
		unsigned long long internalTimestamp = event->timestamp();
		if(m_lastAltInternalTimestamp != internalTimestamp) {
			m_lastAltInternalTimestamp = internalTimestamp;
			long long now = QDateTime::currentMSecsSinceEpoch();
			if(now - m_lastAltPress <= 400) {
				m_lastAltPress = 0;
				emit focusCanvas();
			} else {
				m_lastAltPress = now;
			}
		}
	}
}

#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_ANDROID)
void GlobalKeyEventFilter::updateEraserNear(bool near)
{
	if(near != m_eraserNear) {
		m_eraserNear = near;
		emit eraserNear(near);
	}
}
#endif
