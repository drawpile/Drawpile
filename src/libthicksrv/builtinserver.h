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

#ifndef DP_BUILTIN_SERVER_H
#define DP_BUILTIN_SERVER_H

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

namespace canvas {
    class StateTracker;
	class AclFilter;
}

class ZeroConfAnnouncement;

namespace server {

class BuiltinSession;
class Session;
class Client;
class ServerConfig;
class ServerLog;

/**
 * A server built-in to the client application.
 */
class BuiltinServer : public QObject, public Sessions {
	Q_OBJECT
public:
	explicit BuiltinServer(canvas::StateTracker *statetracker, const canvas::AclFilter *aclFilter, QObject *parent=nullptr);
	~BuiltinServer();

	ServerConfig *config() { return m_config; }

	/**
	 * @brief Get the actual port the server is running from
	 * @return port number or zero if server is not running
	 */
	quint16 port() const;

	QJsonArray sessionDescriptions() const override;
	Session *getSessionById(const QString &id, bool loadTemplate) override;
	std::tuple<Session*, QString> createSession(const QString &id, const QString &idAlias, const protocol::ProtocolVersion &protocolVersion, const QString &founder) override;

	/**
	 * Start the server.
	 *
	 * If the preferred port is not available, a random port number is selected.
	 */
	bool start(QString *errorMessage);

public slots:

	 //! Stop the server. All clients are disconnected.
	void stop();

	void doInternalReset();

private slots:
	void newClient();
	void removeClient(QObject *clientObj);
	void onSessionAttributeChange();

signals:
	void serverStopped();

private:
	ServerConfig *m_config = nullptr;
	QTcpServer *m_server = nullptr;
	sessionlisting::Announcements *m_announcements = nullptr;

	QList<Client*> m_clients;
	BuiltinSession *m_session = nullptr;

	canvas::StateTracker *m_statetracker;
	const canvas::AclFilter *m_aclFilter;

	ZeroConfAnnouncement *m_zeroconfAnnouncement = nullptr;

	int m_forwardedPort = 0;
};

}

#endif

