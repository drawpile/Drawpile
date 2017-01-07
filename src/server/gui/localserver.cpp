/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#include "localserver.h"
#include "multiserver.h"
#include "../shared/util/whatismyip.h"

namespace server {
namespace gui {

LocalServer::LocalServer(MultiServer *server, QObject *parent)
	: Server(parent), m_server(server)
{
	Q_ASSERT(server);

	connect(server, &MultiServer::serverStartError, this, &LocalServer::serverError);
	connect(server, &MultiServer::serverStarted, this, &LocalServer::serverStateChanged);
	connect(server, &MultiServer::serverStopped, this, &LocalServer::serverStateChanged);
}

QString LocalServer::address() const
{
	QString addr = m_server->announceLocalAddr();
	if(addr.isEmpty())
		addr = WhatIsMyIp::instance()->myAddress();
	return addr;
}

int LocalServer::port() const
{
	const int p = m_server->port();
	return p;
}

bool LocalServer::isRunning() const
{
	bool result;
	QMetaObject::invokeMethod(
		m_server, "isRunning", Qt::BlockingQueuedConnection,
		Q_RETURN_ARG(bool, result)
		);
	return result;
}

void LocalServer::startServer()
{
	if(isRunning()) {
		qWarning("Tried to start a server that was already running!");
		return;
	}

	// These settings are safe to set from another thread when the server isn't running
#if 0
	m_server->setSslCertFile(cert, key);
	m_server->setMustSecure(false);
	m_server->setAnnounceLocalAddr(QString());
	m_server->setRecordingPath(QString());
#endif

	// Start the server
	quint16 port = 27750;

	QMetaObject::invokeMethod(
		m_server, "start", Qt::QueuedConnection,
		Q_ARG(quint16, port)
		);
}

void LocalServer::stopServer()
{
	// Calling stop is safe in any state
	QMetaObject::invokeMethod(m_server, "stop", Qt::QueuedConnection);
}

}
}
