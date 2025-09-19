// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/chat/chatlineeditmobile.h"
#include <QKeyEvent>

ChatLineEdit::ChatLineEdit(QWidget *parent)
	: QLineEdit(parent)
{
	connect(
		this, &QLineEdit::textChanged, this,
		&ChatLineEdit::updateMessageAvailable);
	connect(this, &QLineEdit::returnPressed, this, &ChatLineEdit::sendMessage);
}

void ChatLineEdit::sendMessage()
{
	QString str = text();
	if(!str.trimmed().isEmpty()) {
		emit messageSent(str);
		setText(QString());
	}
}

void ChatLineEdit::updateMessageAvailable()
{
	bool available = text().trimmed().isEmpty();
	if(available != m_wasMessageAvailable) {
		m_wasMessageAvailable = available;
		emit messageAvailable(available);
	}
}
