// SPDX-License-Identifier: GPL-3.0-or-later
#include "builtinserver.h"
#include "libclient/server/builtinclient.h"
#include "libclient/server/builtinsession.h"
#include "libserver/announcements.h"
#include "libserver/client.h"
#include "libserver/inmemoryconfig.h"
#include "libserver/loginhandler.h"
#include "libshared/net/proxy.h"
#include <QJsonArray>
#include <QNetworkProxy>
#include <QTcpServer>

namespace server {

BuiltinServer::BuiltinServer(canvas::PaintEngine *paintEngine, QObject *parent)
	: QObject{parent}
	, m_paintEngine{paintEngine}
	, m_config{initConfig()}
	, m_announcements{new sessionlisting::Announcements{m_config, this}}
{
}

quint16 BuiltinServer::port() const
{
	return m_server ? m_server->serverPort() : 0;
}

QJsonArray BuiltinServer::sessionDescriptions() const
{
	if(m_session) {
		return {m_session->getDescription()};
	} else {
		return {};
	}
}

Session *BuiltinServer::getSessionById(const QString &id, bool loadTemplate)
{
	Q_UNUSED(loadTemplate);
	return matchesSession(id) ? m_session : nullptr;
}

QJsonObject BuiltinServer::getSessionDescriptionByIdOrAlias(
	const QString &idOrAlias, bool loadTemplate)
{
	Q_UNUSED(loadTemplate);
	return matchesSession(idOrAlias) ? m_session->getDescription()
									 : QJsonObject();
}

std::tuple<Session *, QString> BuiltinServer::createSession(
	const QString &id, const QString &idAlias,
	const protocol::ProtocolVersion &protocolVersion, const QString &founder)
{
	if(m_session != nullptr) {
		return {nullptr, QStringLiteral("closed")};
	}

	if(!protocolVersion.isCurrent()) {
		return {nullptr, QStringLiteral("badProtocol")};
	}

	m_session = new BuiltinSession(
		m_config, m_announcements, m_paintEngine, id, idAlias, founder, this);

	return {m_session, QString{}};
}

bool BuiltinServer::start(
	quint16 preferredPort, int clientTimeout, int proxyMode,
	QString *outErrorMessage)
{
	Q_ASSERT(!m_server);

	m_server = new QTcpServer{this};
	if(net::shouldDisableProxy(proxyMode, m_server->proxy(), false, true)) {
		m_server->setProxy(QNetworkProxy::NoProxy);
	}
	connect(
		m_server, &QTcpServer::newConnection, this, &BuiltinServer::newClient);

	// Try to listen on the given port, fall back to a random one.
	bool ok = m_server->listen(QHostAddress::Any, preferredPort) ||
			  m_server->listen(QHostAddress::Any, 0);
	if(!ok) {
		QString error = m_server->errorString();
		qCritical("Error starting builtin server: %s", qUtf8Printable(error));
		if(outErrorMessage) {
			*outErrorMessage = error;
		}
		delete m_server;
		m_server = nullptr;
		return false;
	}

	InternalConfig icfg = m_config->internalConfig();
	quint16 actualPort = port();
	icfg.realPort = actualPort;
	m_config->setInternalConfig(icfg);
	m_config->setConfigInt(config::ClientTimeout, clientTimeout);

	qInfo("Builtin server started listening on port %d", actualPort);

	return true;
}

void BuiltinServer::stop()
{
	if(m_server) {
		if(m_session) {
			m_session->unlistAnnouncement(QUrl(), false);
		}

		delete m_server;
		m_server = nullptr;

		for(Client *c : m_clients) {
			c->disconnectClient(
				Client::DisconnectionReason::Shutdown, QString(), QString());
		}

		delete m_session;
		m_session = nullptr;
	}
}

void BuiltinServer::doInternalReset(const drawdance::CanvasState &canvasState)
{
	if(m_session) {
		m_session->doInternalReset(canvasState);
	}
}

void BuiltinServer::newClient()
{
	QTcpSocket *socket = m_server->nextPendingConnection();
	qInfo(
		"New client connected to builtin server (%s)",
		qUtf8Printable(socket->peerAddress().toString()));

	BuiltinClient *client = new BuiltinClient{socket, m_config->logger(), this};
	client->setConnectionTimeout(
		m_config->getConfigTime(config::ClientTimeout) * 1000);

	m_clients.append(client);
	connect(
		client, &BuiltinClient::builtinClientDestroyed, this,
		&BuiltinServer::removeClient);

	LoginHandler *login = new LoginHandler(client, this, m_config);
	login->startLoginProcess();
}

void BuiltinServer::removeClient(BuiltinClient *client)
{
	m_clients.removeOne(client);
	if(m_clients.isEmpty()) {
		qDebug("Last client disconnected from builtin server, deleting it");
		if(m_session) {
			m_session->unlistAnnouncement(QUrl(), false);
		}
		deleteLater();
	}
}

ServerConfig *BuiltinServer::initConfig()
{
	ServerConfig *config = new InMemoryConfig{this};
	config->setConfigInt(config::SessionSizeLimit, 0);
	config->setConfigInt(config::AutoresetThreshold, 0);
	config->setConfigInt(config::SessionCountLimit, 1);
	return config;
}

bool BuiltinServer::matchesSession(const QString &idOrAlias) const
{
	return !idOrAlias.isEmpty() && m_session &&
		   (m_session->id() == idOrAlias || m_session->idAlias() == idOrAlias);
}

}
