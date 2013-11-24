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

#include "../util/idlist.h"
#include "../net/messagestream.h"

#include "session.h"

class QTcpServer;

namespace server {

class Client;

/**
 * The drawpile server.
 */
class Server : public QObject {
Q_OBJECT
public:
	static const int MAXCLIENTS = 255;

	Server(QObject *parent=0);
	~Server();

	//! Set the stream where error messages are written
	void setErrorStream(QTextStream *stream) { _errors = stream; }

	//! Set the stream where debug messages are written
	void setDebugStream(QTextStream *stream) { _debug = stream; }

	//! Start the server.
	bool start(quint16 port, const QHostAddress& address = QHostAddress::Any);

	/**
	 * @brief Is there a session
	 *
	 * A session is considered to be started after the first user has logged in.
	 * @return true if session started
	 */
	bool isSessionStarted() const { return _hasSession; }

	/**
	 * @brief get the main command stream
	 * @return reference to main command stream
	 */
	const protocol::MessageStream &mainstream() const { return _mainstream; }

	/**
	 * @brief Add a command to the message stream.
	 *
	 * Emits newCommandsAvailable
	 * @param msg
	 */
	void addToCommandStream(protocol::MessagePtr msg);

	/**
	 * @brief Add a new snapshot point.
	 * @pre there are no unfinished snapshot points
	 */
	void addSnapshotPoint();

	/**
	 * @brief Add a message to the latest snapshot point.
	 * @param msg
	 * @pre there is an unfinished snapshot point
	 * @return true if this was the command that completed the snapshot
	 */
	bool addToSnapshotStream(protocol::MessagePtr msg);

	void startSession() { _hasSession = true; }

	SessionState &session() { return _session; }

	/**
	 * @brief Get the number of logged in users.
	 * @return number of logged in users
	 */
	int userCount() const;

	/**
	 * @brief Get the client with the specified ID
	 *
	 * Client must be a logged in member of the session
	 * @param id client ID
	 * @return client or 0 if not found
	 */
	Client *getClientById(int id);

	void printError(const QString &message);
	void printDebug(const QString &message);

public slots:
	 //! Stop the server. All clients are disconnected.
	void stop();

private slots:
	void newClient();
	void removeClient(Client *client);
	void clientLoggedIn(Client *client);

signals:
	//! This signal is emitted when the server becomes empty
	void lastClientLeft();

	//! New commands have been added to the main stream
	void newCommandsAvailable();

	//! A new snapshot was just created
	void snapshotCreated();

private:
	QTcpServer *_server;
	QList<Client*> _clients;

	QTextStream *_errors;
	QTextStream *_debug;

	protocol::MessageStream _mainstream;

	bool _hasSession;
	SessionState _session;
};

}

#endif

