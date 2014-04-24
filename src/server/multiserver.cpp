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

#include "../shared/net/snapshot.h"

namespace server {

MultiServer::MultiServer(QObject *parent)
	: QObject(parent),
	_server(0),
	_state(NOT_STARTED),
	_autoStop(false)
{
	_sessions = new SessionServer(this);

	connect(_sessions, SIGNAL(sessionEnded(int)), this, SLOT(tryAutoStop()));
	connect(_sessions, SIGNAL(userLoggedIn()), this, SLOT(printStatusUpdate()));
	connect(_sessions, &SessionServer::userDisconnected, [this]() {
		printStatusUpdate();
		// The server will be fully stopped after all users have disconnected
		if(_state == STOPPING)
			stop();
		else
			tryAutoStop();
	});

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

/**
 * @brief Set the maximum number of sessions
 *
 * Setting this to 1 disables the "multisession" capability flag. Clients
 * will not display the session selector dialog in that case, but will automatically
 * connect to the only session.
 * @param limit
 */
void MultiServer::setSessionLimit(int limit)
{
	_sessions->setSessionLimit(limit);
}

/**
 * @brief Enable or disable persistent sessions
 * @param persistent
 */
void MultiServer::setPersistentSessions(bool persistent)
{
	_sessions->setAllowPersistentSessions(persistent);
}

/**
 * @brief Automatically stop server when last session is closed
 *
 * This is used in socket activation mode. The server will be restarted
 * by the system init daemon when needed again.
 * @param autostop
 */
void MultiServer::setAutoStop(bool autostop)
{
	_autoStop = autostop;
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
 * @brief Accept or reject new client connection
 */
void MultiServer::newClient()
{
	QTcpSocket *socket = _server->nextPendingConnection();

	logger::info() << "Accepted new client from address" << socket->peerAddress();

	_sessions->addClient(new Client(socket));

	printStatusUpdate();
}


void MultiServer::printStatusUpdate()
{
	initsys::notifyStatus(QString("%1 users and %2 sessions")
		.arg(_sessions->totalUsers())
		.arg(_sessions->sessionCount())
	);
}

/**
 * @brief Stop the server if vacant (and autostop is enabled)
 */
void MultiServer::tryAutoStop()
{
	if(_state == RUNNING && _autoStop && _sessions->sessionCount() == 0 && _sessions->totalUsers() == 0) {
		logger::info() << "Autostopping due to lack of sessions";
		stop();
	}
}

/**
 * Disconnect all clients and stop listening.
 */
void MultiServer::stop() {
	if(_state == RUNNING) {
		logger::info() << "Stopping server and kicking out" << _sessions->totalUsers() << "users...";
		_state = STOPPING;
		_server->close();

		_sessions->stopAll();
	}

	if(_state == STOPPING) {
		if(_sessions->totalUsers() == 0) {
			_state = STOPPED;
			logger::info() << "Server stopped.";
			emit serverStopped();
		}
	}
}

}
