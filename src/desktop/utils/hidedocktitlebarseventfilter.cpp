// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/hidedocktitlebarseventfilter.h"
#include <QApplication>
#include <QKeyEvent>

HideDockTitleBarsEventFilter::HideDockTitleBarsEventFilter(QObject *parent)
	: QObject{parent}
	, m_wasHidden{false}
{
}

bool HideDockTitleBarsEventFilter::eventFilter(QObject *watched, QEvent *event)
{
	switch(event->type()) {
	case QEvent::Enter:
	case QEvent::Leave: {
		checkDockTitleBarsHidden(QApplication::queryKeyboardModifiers());
		break;
	}
	case QEvent::KeyPress:
	case QEvent::KeyRelease:
	case QEvent::ShortcutOverride:
		checkDockTitleBarsHidden(static_cast<QKeyEvent *>(event)->modifiers());
		break;
	default:
		break;
	}
	return QObject::eventFilter(watched, event);
}

void HideDockTitleBarsEventFilter::checkDockTitleBarsHidden(
	Qt::KeyboardModifiers mods)
{
	bool hidden = mods.testFlag(Qt::ShiftModifier);
	if(hidden != m_wasHidden) {
		m_wasHidden = hidden;
		emit setDockTitleBarsHidden(hidden);
	}
}
