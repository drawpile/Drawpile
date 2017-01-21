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

#include <QSettings>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>

namespace server {
namespace gui {

LocalServer::LocalServer(MultiServer *server, QObject *parent)
	: Server(parent), m_server(server)
{
	Q_ASSERT(server);

	connect(server, &MultiServer::serverStartError, this, &LocalServer::serverError);
	connect(server, &MultiServer::serverStarted, this, &LocalServer::serverStateChanged);
	connect(server, &MultiServer::serverStopped, this, &LocalServer::serverStateChanged);
	connect(server, &MultiServer::jsonApiResult, this, &LocalServer::onApiResponse);
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
	int p = m_server->port();
	if(p==0)
		p = QSettings().value("guiserver/port", "27750").toInt();
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
	QSettings cfg;
	cfg.beginGroup("guiserver");

	if(cfg.value("use-ssl", false).toBool()) {
		m_server->setSslCertFile(cfg.value("sslcert").toString(), cfg.value("sslkey").toString());
		m_server->setMustSecure(cfg.value("force-ssl", false).toBool());
	} else {
		m_server->setSslCertFile(QString(), QString());
		m_server->setMustSecure(false);
	}

	m_server->setAnnounceLocalAddr(cfg.value("local-address").toString());

	if(cfg.value("session-storage").toString() == "file") {
		QDir sessionDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/sessions";
		if(!sessionDir.mkpath(".")) {
			qWarning("Couldn't create session directory");
		} else {
			m_server->setSessionDirectory(sessionDir);
		}
	}

	// Start the server
	quint16 port = cfg.value("port", 27750).toInt();

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

void LocalServer::makeApiRequest(const QString &requestId, JsonApiMethod method, const QStringList &path, const QJsonObject request)
{
	// Note: we can call the internal server's JSON API even when the server is stopped
	QMetaObject::invokeMethod(
		m_server, "callJsonApiAsync", Qt::QueuedConnection,
		Q_ARG(QString, requestId),
		Q_ARG(JsonApiMethod, method),
		Q_ARG(QStringList, path),
		Q_ARG(QJsonObject, request)
		);
}

}
}
