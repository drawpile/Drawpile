/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QSettings>

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

	// Set configurable settings
	QSettings cfg;
	cfg.beginGroup("settings/server");

	_sessions->setHistoryLimit(qMax(0, int(cfg.value("historylimit", 0).toDouble() * 1024 * 1024)));
	_sessions->setConnectionTimeout(cfg.value("timeout", 60).toInt() * 1000);
	_sessions->setPrivateUserList(cfg.value("privateUserList", false).toBool());

	// Only one session per server is supported here
	_sessions->setSessionLimit(1);
	connect(_sessions, SIGNAL(sessionEnded(QString)), this, SLOT(stop()));
	connect(_sessions, &SessionServer::userDisconnected, [this]() {
		// The server will be fully stopped after all users have disconnected
		if(_state == STOPPING)
			stop();
	});
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
		_error = _server->errorString();
		logger::error() << "Error starting server:" << _error;
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
 * @brief Accept or reject new client connection
 */
void BuiltinServer::newClient()
{
	QTcpSocket *socket = _server->nextPendingConnection();

	logger::info() << "Accepted new client from address" << socket->peerAddress();

	_sessions->addClient(new Client(socket));
}

/**
 * Disconnect all clients and stop listening.
 */
void BuiltinServer::stop() {
	if(_state == RUNNING) {
		logger::debug() << "Stopping built-in server...";
		_state = STOPPING;

		_server->close();
		_sessions->stopAll();
	}

	if(_state == STOPPING) {
		if(_sessions->totalUsers() == 0) {
			_state = STOPPED;
			logger::debug() << "Built-in server stopped.";
			emit serverStopped();
		}
	}
}

}
