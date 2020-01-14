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

#include "thickserver.h"
#include "thicksession.h"
#include "thickserverclient.h"

#include "../libserver/sslserver.h"
#include "../libserver/serverconfig.h"
#include "../libserver/serverlog.h"
#include "../libserver/loginhandler.h"
#include "../libserver/announcements.h"

#include <QTcpSocket>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QJsonObject>
#include <QJsonArray>

namespace server {

ThickServer::ThickServer(ServerConfig *config, QObject *parent)
	: QObject(parent), m_config(config)
{
	m_announcements = new sessionlisting::Announcements(config, this);
	m_started = QDateTime::currentDateTimeUtc();
}

/**
 * @brief Automatically stop server when last session is closed
 *
 * This is used in socket activation mode. The server will be restarted
 * by the system init daemon when needed again.
 * @param autostop
 */
void ThickServer::setAutoStop(bool autostop)
{
	m_autoStop = autostop;
}

void ThickServer::setRecordingPath(const QString &path)
{
	m_recordingPath = path;
}

bool ThickServer::createServer()
{
	if(!m_sslCertFile.isEmpty() && !m_sslKeyFile.isEmpty()) {
		SslServer *server = new SslServer(m_sslCertFile, m_sslKeyFile, this);
		if(!server->isValidCert()) {
			emit serverStartError("Couldn't load TLS certificate");
			return false;
		}
		m_server = server;

	} else {
		m_server = new QTcpServer(this);
	}

	connect(m_server, &QTcpServer::newConnection, this, &ThickServer::newClient);

	return true;
}

/**
 * @brief Start listening on the specified address.
 * @param port the port to listen on
 * @param address listening address
 * @return true on success
 */
bool ThickServer::start(quint16 port, const QHostAddress& address) {
	Q_ASSERT(m_state == State::Stopped);
	m_state = State::Running;
	if(!createServer()) {
		delete m_server;
		m_server = nullptr;
		m_state = State::Stopped;
		return false;
	}

	if(!m_server->listen(address, port)) {
		emit serverStartError(m_server->errorString());
		Log()
			.about(Log::Level::Error, Log::Topic::Status)
			.message(m_server->errorString())
			.to(m_config->logger());

		delete m_server;
		m_server = nullptr;
		m_state = State::Stopped;
		return false;
	}

	m_port = m_server->serverPort();

	InternalConfig icfg = m_config->internalConfig();
	icfg.realPort = m_port;
	m_config->setInternalConfig(icfg);

	Log()
		.about(Log::Level::Info, Log::Topic::Status)
		.message(QString("Started listening on port %1 at address %2").arg(port).arg(address.toString()))
		.to(m_config->logger());

	emit serverStarted();

	return true;
}

/**
 * @brief Start listening on the given file descriptor
 * @param fd
 * @return true on success
 */
bool ThickServer::startFd(int fd)
{
	Q_ASSERT(m_state == State::Stopped);
	m_state = State::Running;
	if(!createServer())
		return false;

	if(!m_server->setSocketDescriptor(fd)) {
		Log()
			.about(Log::Level::Error, Log::Topic::Status)
			.message("Couldn't set server socket descriptor!")
			.to(m_config->logger());

		delete m_server;
		m_server = nullptr;
		m_state = State::Stopped;
		return false;
	}

	m_port = m_server->serverPort();

	Log()
		.about(Log::Level::Info, Log::Topic::Status)
		.message(QString("Started listening on passed socket"))
		.to(m_config->logger());

	return true;
}

/**
 * @brief Accept or reject new client connection
 */
void ThickServer::newClient()
{
	QTcpSocket *socket = m_server->nextPendingConnection();

	Log()
		.about(Log::Level::Info, Log::Topic::Status)
		.user(0, socket->peerAddress(), QString())
		.message(QStringLiteral("New client connected"))
		.to(m_config->logger());

	auto *client = new ThickServerClient(socket, m_config->logger());

	if(m_config->isAddressBanned(socket->peerAddress())) {
		client->log()
			.about(Log::Level::Warn, Log::Topic::Kick)
			.message("Kicking banned user straight away")
			.to(m_config->logger());

		client->disconnectClient(ThickServerClient::DisconnectionReason::Kick, "BANNED");

	} else {
		client->setParent(this);
		client->setConnectionTimeout(m_config->getConfigTime(config::ClientTimeout) * 1000);

		m_clients.append(client);
		connect(client, &ThickServerClient::destroyed, this, &ThickServer::removeClient);

		auto *login = new LoginHandler(client, this, m_config);
		login->startLoginProcess();
	}
}

void ThickServer::removeClient(QObject *client)
{
	m_clients.removeOne(static_cast<ThickServerClient*>(client));

	if(m_clients.isEmpty() && m_autoStop)
		stop();
}

void ThickServer::sessionEnded()
{
	m_session = nullptr;
}

QJsonArray ThickServer::sessionDescriptions() const
{
	QJsonArray descs;
	if(m_session)
		descs << m_session->getDescription();
	return descs;
}

Session *ThickServer::getSessionById(const QString &id, bool loadTemplate)
{
	Q_UNUSED(loadTemplate)

	if(!id.isEmpty() && m_session && (m_session->id() == id || m_session->idAlias() == id))
		return m_session;

	return nullptr;
}


std::tuple<Session*, QString> ThickServer::createSession(const QString &id, const QString &idAlias, const protocol::ProtocolVersion &protocolVersion, const QString &founder)
{
	Q_ASSERT(!id.isNull());

	if(m_session)
		return std::tuple<Session*, QString> { nullptr, "closed" };

	if(!protocolVersion.isCurrent())
		return std::tuple<Session*, QString> { nullptr, "badProtocol" };

	m_session = new ThickSession(m_config, m_announcements, id, idAlias, founder, this);

	connect(m_session, &ThickSession::destroyed, this, &ThickServer::sessionEnded);

	QString aka = idAlias.isEmpty() ? QString() : QStringLiteral(" (AKA %1)").arg(idAlias);

	m_session->log(Log()
		.about(Log::Level::Info, Log::Topic::Status)
		.message("Session" + aka + " created by " + founder)
		);

	return std::tuple<Session*, QString> {m_session, QString() };
}

/**
 * Disconnect all clients and stop listening.
 */
void ThickServer::stop() {
	if(m_state == State::Running) {
		Log()
			.about(Log::Level::Info, Log::Topic::Status)
			.message(QString("Stopping server and kicking out %1 users...")
				 .arg(m_clients.size())
			)
			.to(m_config->logger());

		m_state = State::Stopping;
		m_server->close();
		m_port = 0;

		for(ThickServerClient *client : m_clients)
			client->disconnectClient(ThickServerClient::DisconnectionReason::Shutdown, QString());

		// TODO
		//m_session->stop();
	}

	if(m_state == State::Stopping) {
		if(m_clients.isEmpty()) {
			m_state = State::Stopped;
			delete m_server;
			m_server = nullptr;
			Log()
				.about(Log::Level::Info, Log::Topic::Status)
				.message("Server stopped.")
				.to(m_config->logger());

			emit serverStopped();
		}
	}
}

}
