/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2018 Calle Laakkonen

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

#include "libserver/jsonapi.h"

#include <QObject>
#include <QHostAddress>
#include <QDateTime>

class QTcpServer;
class QDir;

namespace server {

class Client;
class Session;
class SessionServer;
class ServerConfig;

/**
 * The drawpile server.
 */
class MultiServer : public QObject {
Q_OBJECT
public:
	explicit MultiServer(ServerConfig *config, QObject *parent=0);

	void setSslCertFile(const QString &certfile, const QString &keyfile) { m_sslCertFile = certfile; m_sslKeyFile = keyfile; }
	void setAutoStop(bool autostop);
	void setRecordingPath(const QString &path);
	void setSessionDirectory(const QDir &dir);
	void setTemplateDirectory(const QDir &dir);

#ifndef NDEBUG
	void setRandomLag(uint lag);
#endif

	/**
	 * @brief Get the port the server is running from
	 * @return port number or zero if server is not running
	 */
	int port() const { return m_port; }

	Q_INVOKABLE bool isRunning() const { return m_state != STOPPED; }

	//! Start the server with the given socket descriptor
	bool startFd(int fd);

	SessionServer *sessionServer() { return m_sessions; }

	ServerConfig *config() { return m_config; }

public slots:
	//! Start the server on the given port and listening address
	bool start(quint16 port, const QHostAddress& address = QHostAddress::Any);

	 //! Stop the server. All clients are disconnected.
	void stop();

	/**
	 * @brief Call the server's JSON administration API
	 *
	 * This is used by the HTTP admin API.
	 *
	 * @param method query method
	 * @param path path components
	 * @param request request body content
	 * @return JSON API response content
	 */
	JsonApiResult callJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);

	/**
	 * @brief Like callJsonApi(), but the jsonApiResponse signal will be emitted instead of the return value
	 */
	void callJsonApiAsync(const QString &requestId, JsonApiMethod method, const QStringList &path, const QJsonObject &request);

private slots:
	void newClient();
	void printStatusUpdate();
	void tryAutoStop();
	void assignRecording(Session *session);

signals:
	void serverStartError(const QString &message);
	void serverStarted();
	void serverStopped();
	void jsonApiResult(const QString &serverId, const JsonApiResult &result);
	void userCountChanged(int count);

private:
	bool createServer();

	JsonApiResult serverJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);
	JsonApiResult statusJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);
	JsonApiResult banlistJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);
	JsonApiResult listserverWhitelistJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);
	JsonApiResult accountsJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);
	JsonApiResult logJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);

	enum State {RUNNING, STOPPING, STOPPED};

	ServerConfig *m_config;
	QTcpServer *m_server;
	SessionServer *m_sessions;

	State m_state;

	bool m_autoStop;
	int m_port;

	QString m_sslCertFile;
	QString m_sslKeyFile;
	QString m_recordingPath;

	QDateTime m_started;
};

}

#endif

