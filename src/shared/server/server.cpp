#include <QTcpServer>
#include <QTcpSocket>

#include "server.h"
#include "client.h"
#include "../net/message.h"

namespace server {

Server::Server(QObject *parent) : QObject(parent), _server(0), _lastclient(1),
	_state(NORMAL), _board(1) {
		
}

void Server::start(quint16 port, const QHostAddress& address) {
	Q_ASSERT(_server==0);
	_server = new QTcpServer(this);
	connect(_server, SIGNAL(newConnection()), this, SLOT(newClient()));
	_server->listen(address, port);
}

void Server::stop() {
	foreach(Client *c, _clients) {
		delete c;
	}
	_clients.clear();

	_server->close();
	delete _server;
	_server = 0;
}

/**
 * Accept a connection from a new client.
 */
void Server::newClient() {
	QTcpSocket *socket = _server->nextPendingConnection();
	// Check if we still have room in the server
	if(_clients.count() > MAXCLIENTS) {
		// Server is full
		socket->close();
		delete socket;
		return;
	}
	// Find an available ID for the client
	while(_clients.contains(_lastclient)) {
		if(++_lastclient>MAXCLIENTS)
			_lastclient=1;
	}

	// Add the client
	Client *c = new Client(_lastclient, this, socket);
	connect(c, SIGNAL(disconnected(int)), this, SLOT(killClient(int)));
	connect(c, SIGNAL(lock(int, bool)), this, SLOT(updateLock(int, bool)));
	_clients.insert(_lastclient, c);
}

void Server::killClient(int id) {
	qDebug() << "Client " << id << " disconnected";
	delete _clients.take(id);
	// If the last client leaves, the board state is lost
	if(_clients.size()==0)
		_board = Board(1);
}

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
		// First step. Gracefully lock all users
		foreach(Client *c, _clients) {
			c->syncLock();
		}
		_state = SYNC;

		// Check if everyone is locked already
		updateLock(-1, true);
	}
}

/**
 * Handle changes in a user's lock state.
 * This is used to determine when all users are ready for syncing.
 */
void Server::updateLock(int id, bool state) {
	if(_state == SYNC && _state==true) {
		int locked=0;
		foreach(Client *c, _clients) {
			if(c->isLocked()) ++locked;
		}
		if(locked == _clients.size())
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
		qWarning("Couldn't find a user to get raster data from!");
		foreach(Client *c, _clients) {
			if(c->state()!=Client::ACTIVE)
				c->kick("Internal server error");
		}
	} else {
		_clients.value(id)->requestRaster();
	}
	_state = NORMAL;
}

int Server::redistribute(bool sync, bool active, const QByteArray& data) {
	int count=0;
	foreach(Client *c, _clients) {
		if((c->state()==Client::SYNC && sync) || (c->state()==Client::ACTIVE && active)) {
			c->send(data);
			++count;
		}
	}
	return count;
}

void Server::briefClient(int id) {
	Client *nc = _clients.value(id);
	Q_ASSERT(nc);
	foreach(Client *c, _clients) {
		if(c->state()>Client::LOGIN && c->id()!=id) {
			c->send(protocol::Message(c->toMessage()).serialize());
			// TODO send last received toolinfo
		}
	}
}

}
