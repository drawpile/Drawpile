/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2014 Calle Laakkonen

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

#include <QTcpServer>
#include <QTcpSocket>

#include "multiserver.h"
#include "initsys.h"

#include "../shared/server/session.h"
#include "../shared/server/sessionserver.h"
#include "../shared/server/client.h"
#include "../shared/server/loginhandler.h"

#include "../shared/net/snapshot.h"

namespace server {

MultiServer::MultiServer(QObject *parent)
	: QObject(parent),
	_server(0),
	_state(NOT_STARTED)
{
	_sessions = new SessionServer(this);

	// TODO move this somewhere else?
	connect(_sessions, &SessionServer::sessionCreated, [this](SessionState *session) {
		session->setRecordingFile(_recordingFile);
	});
}

void MultiServer::setHistoryLimit(uint limit)
{
	_sessions->setHistoryLimit(limit);
}

void MultiServer::setHostPassword(const QString &password)
{
	_sessions->setHostPassword(password);
}

void MultiServer::setSessionLimit(int limit)
{
	_sessions->setSessionLimit(limit);
}

void MultiServer::setPersistentSessions(bool persistent)
{
	_sessions->setAllowPersistentSessions(persistent);
}

/**
 * @brief Start listening on the specified address.
 * @param port the port to listen on
 * @param address listening address
 * @return true on success
 */
bool MultiServer::start(quint16 port, const QHostAddress& address) {
	Q_ASSERT(_state == NOT_STARTED);
	_state = RUNNING;
	_server = new QTcpServer(this);

	connect(_server, SIGNAL(newConnection()), this, SLOT(newClient()));

	if(!_server->listen(address, port)) {
		logger::error() << _server->errorString();
		delete _server;
		_server = 0;
		_state = NOT_STARTED;
		return false;
	}

	logger::info() << "Started listening on port" << port << "at address" << address.toString();
	return true;
}

/**
 * @brief Start listening on the given file descriptor
 * @param fd
 * @return true on success
 */
bool MultiServer::startFd(int fd)
{
	Q_ASSERT(_state == NOT_STARTED);
	_state = RUNNING;
	_server = new QTcpServer(this);

	connect(_server, SIGNAL(newConnection()), this, SLOT(newClient()));

	if(!_server->setSocketDescriptor(fd)) {
		logger::error() << "Couldn't set server socket descriptor!";
		delete _server;
		_server = 0;
		_state = NOT_STARTED;
		return false;
	}

	logger::info() << "Started listening on passed socket";
	return true;
}

/**
 * @brief Get the number of connected clients.
 *
 * This includes the clients who haven't yet logged in to a session
 *
 * @return connected client count
 */
int MultiServer::clientCount() const
{
	return _lobby.size() + _sessions->totalUsers();
}

/**
 * @brief Accept or reject new client connection
 */
void MultiServer::newClient()
{
	QTcpSocket *socket = _server->nextPendingConnection();

	logger::info() << "Accepted new client from adderss" << socket->peerAddress().toString();
	logger::info() << "Number of connected clients is now" << clientCount() + 1;

	Client *client = new Client(socket, this);
	_lobby.append(client);

	connect(client, SIGNAL(disconnected(Client*)), this, SLOT(removeClient(Client*)));

	auto *login = new LoginHandler(client, _sessions);
	connect(login, SIGNAL(clientJoined(Client*)), this, SLOT(clientJoined(Client*)));
	login->startLoginProcess();

	printStatusUpdate();
}

void MultiServer::clientJoined(Client *client)
{
	logger::debug() << "client" << client->id() << "moved from lobby to session";
	_lobby.removeOne(client);
	printStatusUpdate();
}

void MultiServer::removeClient(Client *client)
{
	logger::info() << "Client" << client->id() << "from" << client->peerAddress().toString() << "disconnected";
	bool wasInLobby = _lobby.removeOne(client);

	// We are responsible for clients not part of any session
	if(wasInLobby) {
		client->deleteLater();
	}

	printStatusUpdate();

	if(_state == STOPPING)
		stop();
}

/**
 * Disconnect all clients and stop listening.
 */
void MultiServer::stop() {
	if(_state == RUNNING) {
		logger::info() << "Stopping server and kicking out" << clientCount() << "users...";
		_state = STOPPING;
		_server->close();

		for(Client *c : _lobby)
			c->kick(0);

		_sessions->stopAll();
	}

	if(_state == STOPPING) {
		if(clientCount() == 0) {
			_state = STOPPED;
			logger::info() << "Server stopped.";
			emit serverStopped();
		}
	}
}

void MultiServer::printStatusUpdate()
{
	initsys::notifyStatus(QString("%1 users and %2 sessions")
		.arg(clientCount())
		.arg(_sessions->sessionCount())
	);
}

}
