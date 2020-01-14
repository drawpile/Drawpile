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

#ifndef DP_ThickServer_H
#define DP_ThickServer_H

#include "../libserver/sessions.h"

#include <QObject>
#include <QHostAddress>
#include <QDateTime>

class QTcpServer;

namespace protocol {
	class ProtocolVersion;
}

namespace sessionlisting {
	class Announcements;
}

namespace server {

class ThickSession;
class Session;
class ThickServerClient;
class ServerConfig;
class ServerLog;

/**
 * The drawpile server.
 */
class ThickServer : public QObject, public Sessions {
	Q_OBJECT
public:
	explicit ThickServer(ServerConfig *config, QObject *parent=nullptr);

	void setSslCertFile(const QString &certfile, const QString &keyfile) { m_sslCertFile = certfile; m_sslKeyFile = keyfile; }
	void setAutoStop(bool autostop);
	void setRecordingPath(const QString &path);

	/**
	 * @brief Get the port the server is running from
	 * @return port number or zero if server is not running
	 */
	int port() const { return m_port; }

	bool isRunning() const { return m_state != State::Stopped; }

	//! Start the server with the given socket descriptor
	bool startFd(int fd);

	ThickSession *session() { return m_session; }

	ServerConfig *config() { return m_config; }

	QJsonArray sessionDescriptions() const override;

	Session *getSessionById(const QString &id, bool loadTemplate) override;

	std::tuple<Session*, QString> createSession(const QString &id, const QString &idAlias, const protocol::ProtocolVersion &protocolVersion, const QString &founder) override;

public slots:
	//! Start the server on the given port and listening address
	bool start(quint16 port, const QHostAddress& address = QHostAddress::Any);

	 //! Stop the server. All clients are disconnected.
	void stop();

private slots:
	void newClient();
	void removeClient(QObject *clientObj);
	void sessionEnded();

signals:
	void serverStartError(const QString &message);
	void serverStarted();
	void serverStopped();

private:
	enum class State {Running, Stopping, Stopped};

	bool createServer();

	ServerConfig *m_config = nullptr;
	QTcpServer *m_server = nullptr;
	sessionlisting::Announcements *m_announcements;

	QList<ThickServerClient*> m_clients;
	ThickSession *m_session = nullptr;

	State m_state = State::Stopped;

	bool m_autoStop = false;
	int m_port = 0;

	QString m_sslCertFile;
	QString m_sslKeyFile;
	QString m_recordingPath;

	QDateTime m_started;
};

}

#endif

