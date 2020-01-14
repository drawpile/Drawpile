/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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
#include "builtinsession.h"
#include "thickserverclient.h"

#ifdef HAVE_DNSSD
#include "../libshared/listings/zeroconfannouncement.h"
#endif

#ifdef HAVE_UPNP
#include "../libshared/util/upnp.h"
#include "../libshared/util/whatismyip.h"
#endif

#include "../libserver/inmemoryconfig.h"
#include "../libserver/serverlog.h"
#include "../libserver/loginhandler.h"
#include "../libserver/announcements.h"

#include "config.h"

#include <QTcpSocket>
#include <QTcpServer>
#include <QJsonArray>
#include <QSettings>

namespace server {

BuiltinServer::BuiltinServer(canvas::StateTracker *statetracker, const canvas::AclFilter *aclFilter, QObject *parent)
	: QObject(parent),
	  m_statetracker(statetracker),
	  m_aclFilter(aclFilter)
{
	m_config = new InMemoryConfig(this);

	m_config->setConfigInt(config::SessionSizeLimit, 0);
	m_config->setConfigInt(config::AutoresetThreshold, 0);
	m_config->setConfigInt(config::SessionCountLimit, 1);

	m_announcements = new sessionlisting::Announcements(m_config, this);
}

BuiltinServer::~BuiltinServer()
{
#ifdef HAVE_UPNP
	if(m_forwardedPort > 0) {
		qInfo("Removing UPnP port forward (%d)", m_forwardedPort);
		UPnPClient::instance()->deactivateForward(m_forwardedPort);
	}
#endif
}

quint16 BuiltinServer::port() const
{
	if(m_server)
		return m_server->serverPort();
	return 0;
}

bool BuiltinServer::start(QString *errorMessage) {
	Q_ASSERT(m_server == nullptr);

	QSettings cfg;
	cfg.beginGroup("settings/server");

	m_server = new QTcpServer(this);
	connect(m_server, &QTcpServer::newConnection, this, &BuiltinServer::newClient);

	bool ok = m_server->listen(QHostAddress::Any, cfg.value("port", DRAWPILE_PROTO_DEFAULT_PORT).toUInt());

	// Use a random port if preferred port is not available
	if(!ok)
		ok = m_server->listen(QHostAddress::Any, 0);

	if(!ok) {
		if(errorMessage)
			*errorMessage = m_server->errorString();

		qCritical("Error starting server: %s", qPrintable(m_server->errorString()));

		delete m_server;
		m_server = nullptr;
		return false;
	}

	InternalConfig icfg = m_config->internalConfig();
	icfg.realPort = port();
	m_config->setInternalConfig(icfg);

	m_config->setConfigInt(config::ClientTimeout, cfg.value("timeout", config::ClientTimeout.defaultValue).toInt());
	m_config->setConfigBool(config::PrivateUserList, cfg.value("privateUserList", false).toBool());

	qInfo("BuiltinServer: Started listening on port %d", port());

#ifdef HAVE_DNSSD
	if(cfg.value("dnssd", true).toBool()) {
		m_zeroconfAnnouncement = new ZeroConfAnnouncement(this);
		m_zeroconfAnnouncement->publish(port());
	}
#endif

#ifdef HAVE_UPNP
	if(cfg.value("upnp", true).toBool()) {
		const QString myIp = WhatIsMyIp::guessLocalAddress();
		if(WhatIsMyIp::isMyPrivateAddress(myIp)) {
			qInfo("UPnP enabled and %s appears to be a private IP. Trying to forward port %d...", qPrintable(myIp), port());
			m_forwardedPort = port();
			UPnPClient::instance()->activateForward(port());
		}
	}
#endif

	return true;
}

/**
 * @brief Accept or reject new client connection
 */
void BuiltinServer::newClient()
{
	QTcpSocket *socket = m_server->nextPendingConnection();

	qInfo("New client connected (%s)", qPrintable(socket->peerAddress().toString()));

	auto *client = new ThickServerClient(socket, m_config->logger());

	client->setParent(this);
	client->setConnectionTimeout(m_config->getConfigTime(config::ClientTimeout) * 1000);

	m_clients.append(client);
	connect(client, &QObject::destroyed, this, &BuiltinServer::removeClient);

	(new LoginHandler(client, this, m_config))->startLoginProcess();
}

void BuiltinServer::removeClient(QObject *client)
{
	m_clients.removeOne(static_cast<Client*>(client));

	if(m_clients.isEmpty())
		deleteLater();
}

QJsonArray BuiltinServer::sessionDescriptions() const
{
	QJsonArray descs;
	if(m_session)
		descs << m_session->getDescription();
	return descs;
}

Session *BuiltinServer::getSessionById(const QString &id, bool loadTemplate)
{
	Q_UNUSED(loadTemplate)

	if(!id.isEmpty() && m_session && (m_session->id() == id || m_session->idAlias() == id))
		return m_session;

	return nullptr;
}


std::tuple<Session*, QString> BuiltinServer::createSession(const QString &id, const QString &idAlias, const protocol::ProtocolVersion &protocolVersion, const QString &founder)
{
	if(m_session != nullptr)
		return std::tuple<Session*, QString> { nullptr, "closed" };

	if(!protocolVersion.isCurrent())
		return std::tuple<Session*, QString> { nullptr, "badProtocol" };

	m_session = new BuiltinSession(
		m_config,
		m_announcements,
		m_statetracker,
		m_aclFilter,
		id,
		idAlias,
		founder,
		this
		);

	connect(m_session, &Session::sessionAttributeChanged, this, &BuiltinServer::onSessionAttributeChange);

	return std::tuple<Session*, QString> {m_session, QString() };
}

void BuiltinServer::onSessionAttributeChange()
{
#ifdef HAVE_DNSSD
	if(m_zeroconfAnnouncement)
		m_zeroconfAnnouncement->setTitle(m_session->history()->title());
#endif
}

/**
 * Disconnect all clients and stop listening.
 */
void BuiltinServer::stop() {
	if(!m_server)
		return;

	delete m_server;
	m_server = nullptr;

	for(Client *c : m_clients) {
		c->disconnectClient(Client::DisconnectionReason::Shutdown, QString());
	}

	delete m_session;
	m_session = nullptr;
}

void BuiltinServer::doInternalReset()
{
	if(m_session)
		m_session->doInternalResetNow();
}

}
