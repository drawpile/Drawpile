/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#include "chatwidget.h"
#include "ui_chatbox.h"

namespace widgets {

ChatBox::ChatBox(QWidget *parent)
	: QDockWidget(tr("Chat"), parent)
{
	ui_ = new Ui_ChatBox;
	QWidget *w = new QWidget(this);
	setWidget(w);
	ui_->setupUi(w);

	connect(ui_->sendbutton, SIGNAL(clicked()), this, SLOT(sendMessage()));
}

ChatBox::~ChatBox()
{
	delete ui_;
}

void ChatBox::clear()
{
	ui_->chatbox->clear();
}

void ChatBox::joined(const QString& mynick)
{
	mynick_ = mynick;
	ui_->sendbutton->setEnabled(true);
	systemMessage(tr("Joined session"));
}

void ChatBox::parted()
{
	ui_->sendbutton->setEnabled(false);
	systemMessage(tr("Left session"));
}

void ChatBox::sendMessage()
{
	QString msg = ui_->chatline->text().trimmed();
	if(msg.isEmpty())
		return;
	receiveMessage(mynick_, msg);
	emit message(msg);
	ui_->chatline->clear();
}

/**
 * The received message is displayed in the chat box.
 * @param nick nickname of the user who said something
 * @param message what was said
 */
void ChatBox::receiveMessage(const QString& nick, const QString& message)
{
	ui_->chatbox->append(
			"<b>" +
			nick +
			":</b> " +
			message
			);
}

/**
 * @param message the message
 */
void ChatBox::systemMessage(const QString& message)
{
	ui_->chatbox->append(
			"<b> *** " + message + " ***</b>"
			);
}

}

