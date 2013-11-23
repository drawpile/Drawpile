/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#ifndef DP_SERVER_CLIENT_H
#define DP_SERVER_CLIENT_H

#include <QObject>
#include <QHostAddress>

class QTcpSocket;

namespace protocol {
	class MessageQueue;
	class Login;
}

namespace server {

class Server;

class Client : public QObject
{
    Q_OBJECT
	enum State {
		LOGIN,
		WAIT_FOR_SYNC,
		IN_SESSION
	};

public:
	Client(Server *server, QTcpSocket *socket);
	~Client();

	//! Get the user's host address
	QHostAddress peerAddress() const;

	/**
	 * @brief Get the context ID of the client
	 *
	 * This is initially zero until the login process is complete.
	 * @return client ID
	 */
	int id() const { return _id; }

	/**
	 * @brief Set the context ID of the client.
	 *
	 * For non-hosting users, this is assigned by the server at the end of the login.
	 * @param id
	 */
	void setId(int id) { _id = id; }

signals:
	void disconnected(Client *client);
	void loggedin(Client *client);

public slots:

private slots:
	void gotBadData(int len, int type);
	void receiveMessages();
	void socketDisconnect();

private:
	void handleLoginMessage(const protocol::Login &msg);
	void handleHostSession(const QString &msg);
	void handleJoinSession(const QString &msg);

	bool validateUsername(const QString &username);

	Server *_server;
	QTcpSocket *_socket;
	protocol::MessageQueue *_msgqueue;

	State _state;
	int _substate;

	int _id;
	QString _username;
};

}

#endif
