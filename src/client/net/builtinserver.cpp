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
#include <QHostAddress>

#include "builtinserver.h"

#include "../shared/server/client.h"
#include "../shared/server/loginhandler.h"
#include "../shared/server/session.h"
#include "../shared/server/sessionserver.h"

#include "../shared/util/logger.h"
#include "../shared/net/snapshot.h"

namespace server {

BuiltinServer::BuiltinServer(QObject *parent)
	: QObject(parent),
	  _server(0),
	  _state(NOT_STARTED)
{
	_sessions = new SessionServer(this);

	// Only one session per server is supported here
	_sessions->setSessionLimit(1);
	connect(_sessions, SIGNAL(sessionEnded(int)), this, SLOT(stop()));
}

void BuiltinServer::setHistoryLimit(uint limit)
{
	_sessions->setHistoryLimit(limit);
}

/**
 * @brief Start listening
 *
 * If the preferred port is not available, a random port is chosen.
 *
 * @param preferredPort the default port to listen on
 */
bool BuiltinServer::start(quint16 preferredPort) {
	Q_ASSERT(_state == NOT_STARTED);
	_state = RUNNING;
	_server = new QTcpServer(this);

	connect(_server, SIGNAL(newConnection()), this, SLOT(newClient()));

	bool ok = _server->listen(QHostAddress::Any, preferredPort);

	if(!ok)
		ok = _server->listen(QHostAddress::Any, 0);

	if(ok==false) {
		logger::error() << _server->errorString();
		delete _server;
		_server = 0;
		_state = NOT_STARTED;
		return false;
	}

	logger::info() << "Started listening on port" << port();
	return true;
}

/**
 * @brief Get the port the server is listening on
 * @return port number
 */
int BuiltinServer::port() const
{
	Q_ASSERT(_server);
	return _server->serverPort();
}

/**
 * @brief Get the number of connected clients.
 *
 * This includes the clients who haven't yet logged in to the session
 * @return
 */
int BuiltinServer::clientCount() const
{
	return _lobby.size() + _sessions->totalUsers();
}

/**
 * @brief Accept or reject new client connection
 */
void BuiltinServer::newClient()
{
	QTcpSocket *socket = _server->nextPendingConnection();

	logger::info() << "Accepted new client from address" << socket->peerAddress().toString();

	Client *client = new Client(socket, this);
	_lobby.append(client);

	connect(client, SIGNAL(disconnected(Client*)), this, SLOT(removeClient(Client*)));

	auto *login = new LoginHandler(client, _sessions);
	connect(login, SIGNAL(clientJoined(Client*)), this, SLOT(clientJoined(Client*)));
	login->startLoginProcess();
}

void BuiltinServer::clientJoined(Client *client)
{
	_lobby.removeOne(client);
}

void BuiltinServer::removeClient(Client *client)
{
	logger::info() << "Client" << client->id() << "from" << client->peerAddress().toString() << "disconnected";
	bool wasInLobby = _lobby.removeOne(client);
	if(wasInLobby) {
		// If client was not in the lobby, it was part of a session.
		client->deleteLater();
	}

	if(_state == STOPPING)
		stop();
}

/**
 * Disconnect all clients and stop listening.
 */
void BuiltinServer::stop() {
	if(_state == RUNNING) {
		logger::debug() << "Stopping built-in server...";
		_state = STOPPING;

		_server->close();

		for(Client *c : _lobby)
			c->kick(0);

		_sessions->stopAll();
	}

	if(_state == STOPPING) {
		if(clientCount() == 0) {
			_state = STOPPED;
			logger::debug() << "Built-in server stopped.";
			emit serverStopped();
		}
	}
}

}
