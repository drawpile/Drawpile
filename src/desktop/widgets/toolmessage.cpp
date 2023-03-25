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

#include "desktop/widgets/toolmessage.h"
#include <QTimerEvent>

void ToolMessage::showText(const QString &text)
{
	new ToolMessage{text};
}

void ToolMessage::timerEvent(QTimerEvent *e)
{
	killTimer(e->timerId());
	deleteLater();
}

ToolMessage::ToolMessage(const QString &text)
	: QLabel{
		  text, nullptr,
		  Qt::BypassWindowManagerHint | Qt::FramelessWindowHint |
			  Qt::WindowDoesNotAcceptFocus | Qt::Tool}
{
	setAttribute(Qt::WA_TransparentForMouseEvents);
	setContentsMargins(4, 4, 4, 4);
	startTimer(500 + 40 * text.length());
	move(QCursor::pos() + QPoint(1, 1));
	show();
}
