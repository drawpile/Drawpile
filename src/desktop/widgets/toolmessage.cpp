// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/toolmessage.h"
#include <QCoreApplication>
#include <QScreen>
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
	QSize sh = sizeHint();
	QPoint pos = QCursor::pos() - QPoint(sh.width(), sh.height());
	QScreen *s = screen();
	if(s) {
		QRect screenBounds = s->geometry();
		if(!screenBounds.isEmpty()) {
			QRect bounds(pos, sh);
			if(!screenBounds.contains(bounds)) {
				if(bounds.left() < screenBounds.left()) {
					pos.setX(screenBounds.left());
				} else if(bounds.right() > screenBounds.right()) {
					pos.setX(screenBounds.right() - bounds.width() + 1);
				}
				if(bounds.top() < screenBounds.top()) {
					pos.setY(screenBounds.top());
				} else if(bounds.bottom() > screenBounds.bottom()) {
					pos.setY(screenBounds.bottom() - bounds.height() + 1);
				}
			}
		}
	}
	move(pos);
	show();
}
