/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

/**
 * The drawpile server.
 */
class MultiServer : public QObject {
Q_OBJECT
public:
	explicit MultiServer(QObject *parent=0);

	void setHistoryLimit(uint limit);
	void setRecordingFile(const QString &filename) { _recordingFile = filename; }
	void setHostPassword(const QString &password);
	void setSessionLimit(int limit);
	void setPersistentSessions(bool persistent);

	bool start(quint16 port, const QHostAddress& address = QHostAddress::Any);

	int clientCount() const;

public slots:
	 //! Stop the server. All clients are disconnected.
	void stop();

private slots:
	void newClient();
	void clientJoined(Client *client);
	void removeClient(Client *client);

signals:
	void serverStopped();

private:
	enum State {NOT_STARTED, RUNNING, STOPPING, STOPPED};

	QTcpServer *_server;
	QList<Client*> _lobby;
	SessionServer *_sessions;
	State _state;

	QString _recordingFile;
};

}

#endif

