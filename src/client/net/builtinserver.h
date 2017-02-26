/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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

#ifndef DP_BUILTINSERVER_H
#define DP_BUILTINSERVER_H

#include <QObject>

class QTcpServer;

namespace server {

class SessionServer;
class ServerConfig;

/**
 * The drawpile server.
 */
class BuiltinServer : public QObject {
Q_OBJECT
public:
	explicit BuiltinServer(QObject *parent=nullptr);

	bool start(quint16 preferredPort);

	int port() const;

	const QString &errorString() const { return m_error; }

public slots:
	 //! Stop the server. All clients are disconnected.
	void stop();

private slots:
	void newClient();

signals:
	void serverStopped();

private:
	enum State {NOT_STARTED, RUNNING, STOPPING, STOPPED};

	QTcpServer *m_server;
	ServerConfig *m_config;
	SessionServer *m_sessions;
	State m_state;

	QString m_error;
};

}

#endif

