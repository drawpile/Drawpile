/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2013 Calle Laakkonen

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

class QTextBrowser;
class ChatLineEdit;

namespace widgets {

/**
 * @brief Chat window
 *
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
		void receiveMessage(const QString& nick, const QString &message, bool isme=false);

		//! Display a received marker
		void receiveMarker(const QString &nick, const QString &message);

		//! Display a system message
		void systemMessage(const QString& message);

		void userJoined(int id, const QString &name);
		void userParted(const QString &name);

		//! Empty the chat box
		void clear();

	signals:
		void message(const QString& msg);

	private:
		QTextBrowser *_view;
		ChatLineEdit *_myline;
};

}

#endif

