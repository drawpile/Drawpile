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
#if 0
#include "../net/message.h"
#include "../net/toolselect.h"
#endif

namespace server {

Server::Server(QObject *parent) : QObject(parent)
{
	//_errors = new QTextStream(stderr);
}

Server::~Server() {
#if 0
	delete _errors;
	delete _debug;
#endif
}

/**
 * Start listening on the specified address.
 */
bool Server::start(quint16 port, const QHostAddress& address) {
#if 0
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
#endif
	return true;
}

/**
 * Disconnect all clients and stop listening.
 */
void Server::stop() {
#if 0
	clearClients();
	_server->close();
	delete _server;
	_server = 0;
#endif
}

}
