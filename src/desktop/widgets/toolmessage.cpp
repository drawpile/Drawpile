// SPDX-License-Identifier: GPL-3.0-or-later

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
