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
#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>

class Ui_ChatBox;

namespace widgets {

//! Chat window
/**
 * A widget for chatting with other users
 */
class ChatBox: public QWidget
{
	Q_OBJECT
	public:
		ChatBox(QWidget *parent=0);
		~ChatBox();

	public slots:
		//! Display a received message
		void receiveMessage(const QString& nick, const QString &message);

		//! Display a system message
		void systemMessage(const QString& message);

		//! A session was joined
		void joined(const QString& title, const QString& mynick);

		//! A session was left
		void parted();

		//! Empty the chat box
		void clear();

	private slots:
		void sendMessage();

	signals:
		void message(const QString& msg);

	private:
		Ui_ChatBox *ui_;
		QString mynick_;
};

}

#endif

