/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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

#ifndef DP_SERVER_H
#define DP_SERVER_H

#include <QObject>
#include <QHostAddress>
#include <QHash>

class QTcpServer;

namespace server {

class Client;

/**
 * The drawpile server.
 */
class Server : public QObject {
	Q_OBJECT
	static const int MAXCLIENTS = 255;
	public:
		enum State {
			// Business as usual, relay packets.
			NORMAL,
			// Synchronize new users
			SYNC
		};

		Server(QObject *parent=0);

		~Server();

		//! Set the stream where error messages are written
		void setErrorStream(QTextStream *stream);

		//! Set the stream where debug messages are written
		void setDebugStream(QTextStream *stream);

		//! Start the server.
		bool start(quint16 port, const QHostAddress& address = QHostAddress::Any);

	public slots:
		 //! Stop the server. All clients are disconnected.
		void stop();

	signals:
		//! This signal is emitted when the server becomes empty
		void lastClientLeft();

	private:
};

}

#endif

