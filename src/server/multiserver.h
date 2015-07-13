/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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
#include <QHash>

#include "../shared/util/logger.h"

class QTcpServer;

namespace server {

class Client;
class SessionState;
class SessionServer;
class BanList;

/**
 * The drawpile server.
 */
class MultiServer : public QObject {
Q_OBJECT
public:
	explicit MultiServer(QObject *parent=0);

	void setServerTitle(const QString &title);
	void setWelcomeMessage(const QString &message);
	void setHistoryLimit(uint limit);
	void setRecordingFile(const QString &filename) { _recordingFile = filename; }
	void setSslCertFile(const QString &certfile, const QString &keyfile) { _sslCertFile = certfile; _sslKeyFile = keyfile; }
	void setMustSecure(bool secure);
	void setHostPassword(const QString &password);
	void setSessionLimit(int limit);
	void setPersistentSessions(bool persistent);
	void setExpirationTime(uint seconds);
	void setAutoStop(bool autostop);
	bool setHibernation(const QString &directory, bool all, bool autoHibernate);
	bool setUserFile(const QString &path);
	void setAllowGuests(bool allow);
	void setConnectionTimeout(int timeout);
	void setAnnounceWhitelist(const QString &path);
	void setAnnounceLocalAddr(const QString &addr);
	void setBanlist(const QString &path);

#ifndef NDEBUG
	void setRandomLag(uint lag);
#endif

	bool start(quint16 port, const QHostAddress& address = QHostAddress::Any);
	bool startFd(int fd);

	SessionServer *sessionServer() { return _sessions; }

public slots:
	 //! Stop the server. All clients are disconnected.
	void stop();

private slots:
	void newClient();
	void printStatusUpdate();
	void tryAutoStop();
	void assignRecording(SessionState *session);

signals:
	void serverStopped();

private:
	bool createServer();

	enum State {NOT_STARTED, RUNNING, STOPPING, STOPPED};

	QTcpServer *_server;
	SessionServer *_sessions;
	BanList *_banlist;
	State _state;

	bool _autoStop;
	QString _recordingFile;
	QString _sslCertFile;
	QString _sslKeyFile;
};

}

#endif

