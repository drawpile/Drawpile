/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2017 Calle Laakkonen

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

#include "builtinserver.h"

#include "../shared/server/client.h"
#include "../shared/server/loginhandler.h"
#include "../shared/server/session.h"
#include "../shared/server/sessionserver.h"
#include "../shared/server/inmemoryconfig.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QSettings>

namespace server {

BuiltinServer::BuiltinServer(QObject *parent)
	: QObject(parent),
	  m_server(nullptr),
	  m_state(NOT_STARTED)
{
	// Fixed configuration
	ServerConfig *servercfg = new InMemoryConfig(this);
	QSettings cfg;
	cfg.beginGroup("settings/server");

	servercfg->setConfigInt(config::SessionSizeLimit, cfg.value("historylimit", 0).toFloat() * 1024 * 1024);
	servercfg->setConfigInt(config::SessionCountLimit, 1);
	servercfg->setConfigBool(config::PrivateUserList, cfg.value("privateUserList", false).toBool());
	servercfg->setConfigInt(config::ClientTimeout, cfg.value("timeout", 60).toInt());

	m_sessions = new SessionServer(servercfg, this);

	connect(m_sessions, &SessionServer::sessionEnded, this, &BuiltinServer::stop);
	connect(m_sessions, &SessionServer::userDisconnected, [this]() {
		// The server will be fully stopped after all users have disconnected
		if(m_state == STOPPING)
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
	Q_ASSERT(m_state == NOT_STARTED);
	m_state = RUNNING;
	m_server = new QTcpServer(this);

	connect(m_server, &QTcpServer::newConnection, this, &BuiltinServer::newClient);

	bool ok = m_server->listen(QHostAddress::Any, preferredPort);

	if(!ok)
		ok = m_server->listen(QHostAddress::Any, 0);

	if(ok==false) {
		m_error = m_server->errorString();
		qCritical("Error starting server: %s", qPrintable(m_error));
		delete m_server;
		m_server = nullptr;
		m_state = NOT_STARTED;
		return false;
	}

	qInfo("Started listening on port %d", port());
	return true;
}

/**
 * @brief Get the port the server is listening on
 * @return port number
 */
int BuiltinServer::port() const
{
	Q_ASSERT(m_server);
	return m_server->serverPort();
}

/**
 * @brief Accept or reject new client connection
 */
void BuiltinServer::newClient()
{
	QTcpSocket *socket = m_server->nextPendingConnection();

	qInfo("Accepted new client from address %s", qPrintable(socket->peerAddress().toString()));

	m_sessions->addClient(new Client(socket, m_sessions->config()->logger()));
}

/**
 * Disconnect all clients and stop listening.
 */
void BuiltinServer::stop() {
	if(m_state == RUNNING) {
		qDebug("Stopping built-in server...");
		m_state = STOPPING;

		m_server->close();
		m_sessions->stopAll();
	}

	if(m_state == STOPPING) {
		if(m_sessions->totalUsers() == 0) {
			m_state = STOPPED;
			qDebug("Built-in server stopped.");
			emit serverStopped();
		}
	}
}

}
