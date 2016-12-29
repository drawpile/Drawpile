/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2016 Calle Laakkonen

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

#ifndef DP_MULTISERVER_H
#define DP_MULTISERVER_H

#include <QObject>
#include <QHostAddress>

#include "../shared/util/logger.h"

class QTcpServer;

namespace server {

class Client;
class Session;
class SessionServer;
class IdentityManager;
class ServerConfig;

typedef std::function<bool(const QHostAddress &address)> BanListFunc;

/**
 * The drawpile server.
 */
class MultiServer : public QObject {
Q_OBJECT
public:
	explicit MultiServer(ServerConfig *config, QObject *parent=0);

	void setSslCertFile(const QString &certfile, const QString &keyfile) { m_sslCertFile = certfile; m_sslKeyFile = keyfile; }
	void setMustSecure(bool secure);
	void setAutoStop(bool autostop);
	void setIdentityManager(IdentityManager *idman);
	void setAnnounceLocalAddr(const QString &addr);
	void setBanlist(BanListFunc func);

#ifndef NDEBUG
	void setRandomLag(uint lag);
#endif

	bool start(quint16 port, const QHostAddress& address = QHostAddress::Any);
	bool startFd(int fd);

	SessionServer *sessionServer() { return m_sessions; }

public slots:
	 //! Stop the server. All clients are disconnected.
	void stop();

private slots:
	void newClient();
	void printStatusUpdate();
	void tryAutoStop();
	void assignRecording(Session *session);

signals:
	void serverStopped();

private:
	bool createServer();

	enum State {NOT_STARTED, RUNNING, STOPPING, STOPPED};

	ServerConfig *m_config;
	QTcpServer *m_server;
	SessionServer *m_sessions;

	BanListFunc m_banlist; // TODO remove
	State m_state;

	bool m_autoStop;

	QString m_sslCertFile;
	QString m_sslKeyFile;
};

}

#endif

