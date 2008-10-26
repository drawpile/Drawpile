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

#include <QTcpServer>
#include <QTcpSocket>

#include "server.h"
#include "client.h"
#include "../net/message.h"
#include "../net/toolselect.h"

namespace server {

Server::Server(QObject *parent) : QObject(parent), _server(0), _lastclient(1),
	_uniqueIps(false), _maxnamelen(16), _debug(0), _state(NORMAL),
	_clientVer(-1)
{
	_errors = new QTextStream(stderr);
}

Server::~Server() {
	delete _errors;
	delete _debug;
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
		return false;
	}
	_liveclients = 0;
	_lastclient = 1;
	return true;
}

/**
 * Disconnect all clients and stop listening.
 */
void Server::stop() {
	clearClients();
	_server->close();
	delete _server;
	_server = 0;
}

/**
 * Accept a connection from a new client. If the server is already full,
 * the new client is disconnected. The client is given the first
 * available ID.
 */
void Server::newClient() {
	QTcpSocket *socket = _server->nextPendingConnection();
	// Check if we still have room in the server
	if(_clients.count() > MAXCLIENTS || _liveclients+1 >= _board.maxUsers()) {
		printDebug("New client connected, but we are already full.");
		// Server is full
		socket->close();
		delete socket;
		return;
	}
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
	// Find an available ID for the client
	while(_clients.contains(_lastclient)) {
		if(++_lastclient>MAXCLIENTS)
			_lastclient=1;
	}

	Client *c = new Client(_lastclient, this, socket, _board.defaultLock());
	connect(c, SIGNAL(disconnected(int)), this, SLOT(killClient(int)));
	connect(c, SIGNAL(syncReady(int, bool)), this, SLOT(userSync(int, bool)));
	_clients.insert(_lastclient, c);
	++_liveclients;
}

/**
 * The client is removed and cleaned up. If this was the last client,
 * the board is destroyed as well.
 *
 * In the future, perhaps add an option for retaining the board? Would
 * require either one user to contribute a snapshot or keeping a log of
 * the latest snapshot and all drawing commands after it.
 */
void Server::killClient(int id) {
	printDebug("Client " + QString::number(id) + " disconnected.");
	Client *client = _clients.value(id);
	--_liveclients;
	// If the last client leaves, the board state is lost
	if(_liveclients==0) {
		_board.clear();
		_clientVer = -1;
		clearClients();
		emit lastClientLeft();
	} else {
		if(client->state()>=Client::SYNC) {
			QStringList msg;
			msg << "PART" << QString::number(id);
			redistribute(true, true, protocol::Message(msg).serialize());
		}
		// Users who have participated in the drawing are kept around
		// to simplify syncing.
		if(client->hasSentStroke())
			client->makeGhost();
		else
			delete _clients.take(id);
	}
}

/**
 * The client list is searched and true is returned if there
 * is a client with a matching username.
 */
bool Server::hasClient(const QString& name) {
	foreach(Client *c, _clients) {
		if(c->name().compare(name)==0)
			return true;
	}
	return false;
}

/**
 * The following steps are taken during synchronization:
 * 1. All users are sync-locked (if not already locked, they are gracefully locked)
 * 2. When the last unlocked user locks itself, pick a random user and request
 *    a copy of their board contents.
 *    After sending the request, lift the sync-lock and return to normal
 *    operation.
 * 3. Relay received raster chunks to the new user
 */
void Server::syncUsers() {
	if(_state==NORMAL) {
		printDebug("Synchronizing users.");
		// First step. Prepare users for synchronization
		foreach(Client *c, _clients) {
			c->syncLock();
		}
		_state = SYNC;

		// Check if everyone is locked already
		userSync(-1, true);
	}
}

/**
 * When in sync mode and the last unlocked user locks themself,
 * a copy of the raster data is requested.
 */
void Server::userSync(int id, bool state) {
	if(_state == SYNC && _state==true) {
		int ready=0;
		foreach(Client *c, _clients) {
			if(c->isSyncReady()) ++ready;
		}
		if(ready == _clients.size())
			requestRaster();
	}
}

/**
 * This is the final phase of the new user sync. A user is picked and
 * raster data is requested. The user starts sending it and the server
 * state goes back to normal. TODO what if the user refuses?
 */
void Server::requestRaster() {
	// Pick a user
	int id=-1;
	foreach(Client *c, _clients) {
		if(c->state()==Client::ACTIVE) {
			id = c->id();
			break;
		}
	}
	if(id==-1) {
		printError("Couldn't find a user to get raster data from!");
		foreach(Client *c, _clients) {
			if(c->state()!=Client::ACTIVE)
				c->kick("Internal server error");
		}
	} else {
		printDebug("Requesting raster data from " + _clients.value(id)->name());
		_clients.value(id)->requestRaster();
		_board.clearBuffer();
		foreach(Client *c, _clients) {
			c->syncUnlock();
		}
	}
	_state = NORMAL;
}

/**
 * The raw (preserialized) data is sent to all users.
 * @param sync send to users who are still syncing
 * @param active send to active users
 */
int Server::redistribute(bool sync, bool active, const QByteArray& data) {
	int count=0;
	foreach(Client *c, _clients) {
		if(!c->isGhost() && ((c->state()==Client::SYNC && sync) || (c->state()==Client::ACTIVE && active))) {
			c->sendRaw(data);
			++count;
		}
	}
	return count;
}

/**
 * For each user (except the user being briefed), send the
 * user info and the last received tool select message (if any).
 * Also send all board annotations
 * @param id user to brief
 */
void Server::briefClient(int id) {
	Client *nc = _clients.value(id);
	Q_ASSERT(nc);
	foreach(Client *c, _clients) {
		if(c->state()>Client::LOGIN && c->id()!=id) {
			nc->sendRaw(protocol::Message(c->toMessage()).serialize());
			if(c->lastTool().size()>0)
				nc->sendRaw(c->lastTool());
			if(c->lastLayer() >= 0)
				nc->sendRaw(protocol::LayerSelect(c->id(), c->lastLayer()).serialize());
		}
	}
}

/**
 * @param locker id of the user who issued the lock command
 * @param id id of the user to (un)lock
 * @parma status desired lock status
 */
void Server::lockClient(int locker, int id, bool status) {
	if(_board.owner() != locker)
		kickClient(_board.owner(), locker, "not admin");
	if(hasClient(id))
		_clients[id]->lock(status);
}

/**
 * @param kicker id of the user who issued the kick command
 * @param id id of the user to kick
 */
void Server::kickClient(int kicker, int id, const QString& reason) {
	if(_board.owner() != kicker) {
		if(hasClient(kicker))
			_clients[kicker]->kick("not admin");
	}
	if(hasClient(id))
		_clients[id]->kick(reason);
}

/**
 * Delete all clients, even the ghosted ones.
 */
void Server::clearClients() {
	foreach(Client *c, _clients) {
		delete c;
	}
	_clients.clear();
}

void Server::setErrorStream(QTextStream *stream) {
	delete _errors;
	_errors = stream;
}

void Server::setDebugStream(QTextStream *stream) {
	delete _debug;
	_debug = stream;
}

void Server::printError(const QString& message) {
	if(_errors) {
		*_errors << message << '\n';
		_errors->flush();
	}
}

void Server::printDebug(const QString& message) {
	if(_debug) {
		*_debug << message << '\n';
		_debug->flush();
	}
}

}
