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

#include "server.h"
#include "session.h"
#include "client.h"
#include "loginhandler.h"

#include "../net/snapshot.h"

namespace server {

Server::Server(QObject *parent)
	: QObject(parent),
	  _server(0),
	  _logger(SharedLogger(new DummyLogger)),
	  _session(0),
	  _stopping(false),
	  _persistent(false),
	  _historylimit(0)
{
}

Server::~Server()
{
}

/**
 * @brief Start listening on the specified address.
 * @param port the port to listen on
 * @param anyport if true, a random port is tried if the preferred on is not available
 * @param address listening address
 */
bool Server::start(quint16 port, bool anyport, const QHostAddress& address) {
	Q_ASSERT(_server==0);
	_stopping = false;
	_server = new QTcpServer(this);

	connect(_server, SIGNAL(newConnection()), this, SLOT(newClient()));

	bool ok = _server->listen(address, port);

	if(!ok && anyport)
		ok = _server->listen(address, 0);

	if(ok==false) {
		_logger->logError(_server->errorString());
		delete _server;
		_server = 0;
		return false;
	}

	_logger->logDebug(QString("Started listening on port %1 at address %2").arg(port).arg(address.toString()));
	return true;
}

/**
 * @brief Get the port the server is listening on
 * @return port number
 */
int Server::port() const
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
int Server::clientCount() const
{
	int total = _lobby.size();
	if(_session)
		total += _session->userCount();
	return total;
}

/**
 * @brief Accept or reject new client connection
 */
void Server::newClient()
{
	QTcpSocket *socket = _server->nextPendingConnection();

	_logger->logDebug(QString("Accepted new client from adderss %1").arg(socket->peerAddress().toString()));
	_logger->logDebug(QString("Number of connected clients is now %1").arg(clientCount() + 1));

	Client *client = new Client(socket, _logger, this);
	_lobby.append(client);

	connect(client, SIGNAL(disconnected(Client*)), this, SLOT(removeClient(Client*)));

	LoginHandler *lh = new LoginHandler(client, _session, _logger);
	connect(lh, &LoginHandler::sessionCreated, [this](SessionState *session) {
		if(_session) {
			// whoops! someone else beat this user to it!
			delete session;
			return;
		}
		_session = session;
		session->setParent(this);
		_session->setHistoryLimit(_historylimit);
		_session->setRecordingFile(_recordingFile);
		connect(session, SIGNAL(lastClientLeft()), this, SLOT(lastSessionUserLeft()));
	});
	lh->startLoginProcess();
}

void Server::removeClient(Client *client)
{
	_logger->logDebug(QString("Client %1 from %2 disconnected").arg(client->id()).arg(client->peerAddress().toString()));
	bool wasInLobby = _lobby.removeOne(client);
	if(wasInLobby) {
		// If client was not in the lobby, it was part of a session.
		client->deleteLater();
	}
}

void Server::lastSessionUserLeft()
{
	_logger->logDebug("Last user in session left");
	bool hasSnapshot = _session->mainstream().hasSnapshot();

	if(!hasSnapshot || !_persistent) {
		if(hasSnapshot)
			_logger->logWarning("Shutting down due to lack of users");
		else
			_logger->logWarning("Shutting down because session has not snapshot point!");

		// No snapshot and nobody left: session has been lost
		stop();
	}
}

/**
 * Disconnect all clients and stop listening.
 */
void Server::stop() {
	_stopping = true;
	_server->close();

	foreach(Client *c, _lobby)
		c->kick(0);

	if(_session) {
		_session->kickAllUsers();
		_session->stopRecording();
	}

	emit serverStopped();
}

}
