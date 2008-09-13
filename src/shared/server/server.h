/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

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

#include "board.h"

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
		//! Set the server password
		void setPassword(const QString& password) { _password = password; }

		//! Require each client to have an unique IP address
		void setUniqueIps(bool uniq) { _uniqueIps = uniq; }

		//! Set the maximum username length
		void setMaxNameLength(int len) { _maxnamelen = len; }

		//! Get the maximum username length
		int maxNameLength() const { return _maxnamelen; }

		//! Get the server password
		const QString& password() const { return _password; }

		 //! Check if a user with the specified ID exists.
		bool hasClient(int id) { return _clients.contains(id); }

		 //! Check if the user with the specified name is logged in.
		bool hasClient(const QString& name);

		 //! Return the number of clients
		int clients() const { return _clients.size(); }

		 //! Synchronize users so new people can join.
		void syncUsers();

		 //! Get a new client up to speed
		void briefClient(int id);

		//! Lock a client
		void lockClient(int locker, int id, bool status);

		//! Kick a client
		void kickClient(int kicker, int id, const QString& reason="kicked by session owner");

		 //! Send a packet to all users
		int redistribute(bool sync, bool active, const QByteArray& data);

		 //! Get the drawing board used
		Board& board() { return _board; }
		const Board& board() const { return _board; }

		//! Print a message to the error stream
		void printError(const QString& message);

		//! Print a debug message
		void printDebug(const QString& message);

	public slots:
		 //! Stop the server. All clients are disconnected.
		void stop();

	signals:
		//! This signal is emitted when the server becomes empty
		void lastClientLeft();

	private slots:
		// A new client was added
		void newClient();
		// A client was removed
		void killClient(int id);
		// A client's sync state changed
		void userSync(int id, bool state);

	private:
		// Request raster data from some user
		void requestRaster();

		// Delete all clients
		void clearClients();

		QTcpServer *_server;
		QHash<int,Client*> _clients;
		int _liveclients;
		int _lastclient;

		bool _uniqueIps;
		int _maxnamelen;
		QString _password;

		QTextStream *_errors;
		QTextStream *_debug;

		State _state;
		Board _board;
};

}

#endif

