// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/globalkeyeventfilter.h"
#include "desktop/utils/qtguicompat.h"
#include <QApplication>
#include <QDateTime>
#include <QKeyEvent>

GlobalKeyEventFilter::GlobalKeyEventFilter(QObject *parent)
	: QObject{parent}
	, m_wasHidden{false}
	, m_lastAltPress{0}
	, m_lastAltInternalTimestamp{0}
{
}

bool GlobalKeyEventFilter::eventFilter(QObject *watched, QEvent *event)
{
	switch(event->type()) {
	case QEvent::Enter:
	case QEvent::Leave: {
		checkDockTitleBarsHidden(QApplication::queryKeyboardModifiers());
		break;
	}
	case QEvent::KeyRelease:
		checkCanvasFocus(static_cast<QKeyEvent *>(event));
		Q_FALLTHROUGH();
	case QEvent::KeyPress:
	case QEvent::ShortcutOverride:
		checkDockTitleBarsHidden(static_cast<QKeyEvent *>(event)->modifiers());
		break;
	default:
		break;
	}
	return QObject::eventFilter(watched, event);
}

void GlobalKeyEventFilter::checkDockTitleBarsHidden(Qt::KeyboardModifiers mods)
{
	bool hidden = mods.testFlag(Qt::ShiftModifier);
	if(hidden != m_wasHidden) {
		m_wasHidden = hidden;
		emit setDockTitleBarsHidden(hidden);
	}
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
