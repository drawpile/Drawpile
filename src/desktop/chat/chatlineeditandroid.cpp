// SPDX-License-Identifier: GPL-3.0-or-later

#include <QKeyEvent>

#include "desktop/chat/chatlineeditandroid.h"

ChatLineEdit::ChatLineEdit(QWidget *parent)
	: QLineEdit{parent}
{
	connect(this, &QLineEdit::returnPressed, this, &ChatLineEdit::sendMessage);
}

void ChatLineEdit::sendMessage()
{
	QString str = text();
	if(!str.trimmed().isEmpty()) {
		emit messageSent(str);
		setText(QString{});
	}
}
