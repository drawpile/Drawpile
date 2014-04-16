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

#ifndef DP_SERVER_H
#define DP_SERVER_H

#include <QObject>
#include <QHostAddress>

class QTcpServer;

namespace server {

class Client;
class SessionState;

/**
 * The drawpile server.
 */
class Server : public QObject {
Q_OBJECT
public:
	static const int MAXCLIENTS = 255;

	explicit Server(QObject *parent=0);
	~Server();

	bool start(quint16 port, bool anyport=false, const QHostAddress& address = QHostAddress::Any);

	void setHistorylimit(uint limit) { _historylimit = limit; }
	void setPersistent(bool persistent) { _persistent = persistent; }
	void setRecordingFile(const QString &filename) { _recordingFile = filename; }

	int port() const;
	int clientCount() const;
	bool isSessionStarted() const { return _session != 0; }

public slots:
	 //! Stop the server. All clients are disconnected.
	void stop();

private slots:
	void newClient();
	void removeClient(Client *client);
	void lastSessionUserLeft();

signals:
	//! This signal is emitted when the server becomes empty
	void lastClientLeft();

	void serverStopped();

private:
	QTcpServer *_server;

	QList<Client*> _lobby;

	SessionState *_session;
	bool _stopping;
	bool _persistent;
	uint _historylimit;
	QString _recordingFile;
};

}

#endif

