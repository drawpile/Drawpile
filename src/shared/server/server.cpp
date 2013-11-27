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
	  _hasSession(false),
	  _stopping(false)

{
}

Server::~Server()
{
	delete _server;
}

/**
 * Start listening on the specified address.
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
		printError(_server->errorString());
		delete _server;
		_server = 0;
		return false;
	}

	printDebug(QString("Started listening on port %1 at address %2").arg(port).arg(address.toString()));
	return true;
}

int Server::port() const
{
	Q_ASSERT(_server);
	return _server->serverPort();
}

/**
 * @brief Accept or reject new client connection
 */
void Server::newClient()
{
	QTcpSocket *socket = _server->nextPendingConnection();

	printDebug(QString("Accepted new client from adderss %1").arg(socket->peerAddress().toString()));
	printDebug(QString("Number of connected clients is now %1").arg(_clients.size() + 1));

	Client *client = new Client(this, socket);
	_clients.append(client);

	connect(client, SIGNAL(disconnected(Client*)), this, SLOT(removeClient(Client*)));
	connect(client, SIGNAL(loggedin(Client*)), this, SLOT(clientLoggedIn(Client*)));
	connect(client, SIGNAL(barrierLocked()), this, SLOT(userBarrierLocked()));
}

void Server::removeClient(Client *client)
{
	printDebug(QString("Client %1 from %2 disconnected").arg(client->id()).arg(client->peerAddress().toString()));

	client->deleteLater();

	bool removed = _clients.removeOne(client);
	Q_ASSERT(removed);

	// Make sure there is at least one operator in the server
	bool hasOp=false, hasUsers=false;
	foreach(const Client *c, _clients) {
		if(c->id()>0)
			hasUsers=true;
		if(c->isOperator()) {
			hasOp=true;
			break;
		}
	}
	if(!hasOp) {
		// Make the first fully logged in user the new operator
		foreach(Client *c, _clients) {
			if(c->id()>0) {
				c->grantOp();
				break;
			}
		}
	}
	if(!hasUsers) {
		// The last user left the session.
		_session.closed = false;
		addToCommandStream(_session.sessionConf());

		emit lastClientLeft();

		if(!_mainstream.hasSnapshot()) {
			// No snapshot and no one to provide one? The server is gone...
			stop();
		}

	}
}

void Server::clientLoggedIn(Client *client)
{
	connect(this, SIGNAL(newCommandsAvailable()), client, SLOT(sendAvailableCommands()));
}

int Server::userCount() const
{
	int count=0;
	foreach(const Client *c, _clients)
		if(c->id() > 0)
			++count;
	return count;
}

Client *Server::getClientById(int id)
{
	foreach(Client *c, _clients) {
		if(c->id() == id) {
			return c;
		}
	}
	return 0;
}

/**
 * Disconnect all clients and stop listening.
 */
void Server::stop() {
	_stopping = true;
	_server->close();
	delete _server;
	_server = 0;

	foreach(Client *c, _clients)
		c->kick(0);
}

void Server::addToCommandStream(protocol::MessagePtr msg)
{
	// TODO history size limit. Can clear up to lowest stream pointer index.
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

void Server::cleanupCommandStream()
{
	int removed = _mainstream.cleanup();
	printDebug(QString("Cleaned up %1 messages from the command stream.").arg(removed));
}

void Server::startSnapshotSync()
{
	printDebug("Starting snapshot sync!");

	// Barrier lock all clients
	foreach(Client *c, _clients)
		c->barrierLock();
}

void Server::snapshotSyncStarted()
{
	printDebug("Snapshot sync started!");
	// Lift barrier lock
	foreach(Client *c, _clients)
		c->barrierUnlock();
}

void Server::userBarrierLocked()
{
	// Count locked users
	int locked=0;
	foreach(const Client *c, _clients)
		if(c->isHoldLocked() || c->isDropLocked())
			++locked;

	if(locked == _clients.count()) {
		// All locked, we can now send the snapshot sync request
		foreach(Client *c, _clients) {
			if(c->isOperator()) {
				c->requestSnapshot(true);
				break;
			}
		}
	}
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
