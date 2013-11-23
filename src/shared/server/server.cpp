/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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
#include "client.h"

#include "../net/snapshot.h"

namespace server {

Server::Server(QObject *parent)
	: QObject(parent),
	  _server(0),
	  _errors(0),
	  _debug(0),
	  _hasSession(false)
{
}

Server::~Server()
{
	delete _server;
}

/**
 * Start listening on the specified address.
 */
bool Server::start(quint16 port, const QHostAddress& address) {
	Q_ASSERT(_server==0);
	_server = new QTcpServer(this);

	connect(_server, SIGNAL(newConnection()), this, SLOT(newClient()));

	if(_server->listen(address, port)==false) {
		printError(_server->errorString());
		delete _server;
		_server = 0;
		return false;
	}

	printDebug(QString("Started listening on port %1 at address %2").arg(port).arg(address.toString()));
	return true;
}

/**
 * @brief Accept or reject new client connection
 */
void Server::newClient()
{
	QTcpSocket *socket = _server->nextPendingConnection();
#if 0 // TODO
	if(_uniqueIps) {
		foreach(Client *c, _clients) {
			if(c->address() == socket->peerAddress()) {
				printDebug("New client connected, but there is already a connection from " + socket->peerAddress().toString());
				socket->close();
				delete socket;
				return;
			}
		}
	}
#endif
	printDebug(QString("Accepted new client from adderss %1").arg(socket->peerAddress().toString()));
	printDebug(QString("Number of connected clients is now %1").arg(_clients.size() + 1));

	Client *client = new Client(this, socket);
	_clients.append(client);

	connect(client, SIGNAL(disconnected(Client*)), this, SLOT(removeClient(Client*)));
	connect(client, SIGNAL(loggedin(Client*)), this, SLOT(clientLoggedIn(Client*)));
}

void Server::removeClient(Client *client)
{
	printDebug(QString("Client %1 from %2 disconnected").arg(client->id()).arg(client->peerAddress().toString()));

	client->deleteLater();

	bool removed = _clients.removeOne(client);
	Q_ASSERT(removed);
}

void Server::clientLoggedIn(Client *client)
{
	// Assign user ID
	if(client->id()!=0) {
		// self assigned id, must be the session host
		_session.userids.reserve(client->id());
		printDebug(QString("Session host logged in with user ID %1").arg(client->id()));
	} else {
		// New normal user, assign an ID
		client->setId(_session.userids.takeNext());
		printDebug(QString("User from %1 logged in, assigned ID %2").arg(client->peerAddress().toString(), client->id()));
	}

	connect(this, SIGNAL(newCommandsAvailable()), client, SLOT(sendAvailableCommands()));
}

/**
 * Disconnect all clients and stop listening.
 */
void Server::stop() {
	//clearClients();
	_server->close();
	delete _server;
	_server = 0;
}

void Server::addToCommandStream(protocol::MessagePtr msg)
{
	qDebug() << "added to stream" << msg->type();
	_mainstream.append(msg);
	emit newCommandsAvailable();
}

void Server::addSnapshotPoint()
{
	_mainstream.addSnapshotPoint();
	emit snapshotCreated();
}

bool Server::addToSnapshotStream(protocol::MessagePtr msg)
{
	if(!_mainstream.hasSnapshot()) {
		printError("Tried to add a snapshot command, but there is no snapshot point!");
		return true;
	}
	protocol::SnapshotPoint &sp = _mainstream.snapshotPoint().cast<protocol::SnapshotPoint>();
	if(sp.isComplete()) {
		printError("Tried to add a snapshot command, but the snapshot point is already complete!");
		return true;
	}

	sp.append(msg);

	emit newCommandsAvailable();

	return sp.isComplete();
}

void Server::printError(const QString &message)
{
	if(_errors) {
		*_errors << message << '\n';
		_errors->flush();
	}
}

void Server::printDebug(const QString &message)
{
	if(_debug) {
		*_debug << message << '\n';
		_debug->flush();
	}
}

}
