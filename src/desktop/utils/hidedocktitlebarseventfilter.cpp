/*
 * Drawpile - a collaborative drawing program.
 *
 * Copyright (C) 2023 askmeaboutloom
 *
 * Drawpile is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Drawpile is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "hidedocktitlebarseventfilter.h"
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
