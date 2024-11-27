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
