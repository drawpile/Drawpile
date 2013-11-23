/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

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
#ifndef NETSTATUS_H
#define NETSTATUS_H

#include <QWidget>

class QLabel;

namespace widgets {

class PopupMessage;

/**
 * @brief Network connection status widget
 * This widget displays the current status of the connection with the server
 * and the address of the host.
 */
class NetStatus : public QWidget
{
	Q_OBJECT
	public:
		NetStatus(QWidget *parent);
	
	public slots:
		void connectingToHost(const QString& address);
		void loggedIn();
		void hostDisconnected();

#if 0
		void join(const network::User& user);
		void leave(const network::User& user);
		void kicked(const network::User& user);
		void lock(const QString& reason);
		void unlock();
#endif

		void copyAddress();

	signals:
		//! A status message
		void statusMessage(const QString& message);

	private:
		void message(const QString& msg);

		QLabel *label_, *icon_;
		PopupMessage *popup_;
		QString address_;
		QAction *copyaction_;
};

}

#endif

