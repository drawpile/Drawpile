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

#include "../net/message.h"

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

	/**
	 * @brief Request the client to generate a snapshot
	 *
	 * This causes the creation of a new snapshot point on the main stream.
	 */
	void requestSnapshot();

	/**
	 * @brief Is this client's input queue on hold?
	 *
	 * Hold-lock is a type of lock where the input queue is merely put
	 * on hold (some non-drawing commands are still allowed though). Once
	 * the hold-lock is released, the queued commands will be processed.
	 * @return
	 */
	bool isHoldLocked() const;

	/**
	 * @brief Is this client's input queue being ignored?
	 *
	 * Drop lock is a type of lock where all drawing commands are being
	 * dropped.
	 * @return
	 */
	bool isDropLocked() const;

signals:
	void disconnected(Client *client);
	void loggedin(Client *client);

public slots:
	/**
	 * @brief Enqueue all available commands for sending
	 */
	void sendAvailableCommands();

	/**
	 * @brief A new snapshot was just created
	 */
	void snapshotNowAvailable();

private slots:
	void gotBadData(int len, int type);
	void receiveMessages();
	void receiveSnapshot();
	void socketError();
	void socketDisconnect();

private:
	void handleSessionMessage(protocol::MessagePtr msg);
	void handleLoginMessage(const protocol::Login &msg);
	void handleHostSession(const QString &msg);
	void handleJoinSession(const QString &msg);

	bool validateUsername(const QString &username);
	void updateState(protocol::MessagePtr msg);

	void enqueueHeldCommands();

	Server *_server;
	QTcpSocket *_socket;
	protocol::MessageQueue *_msgqueue;
	QList<protocol::MessagePtr> _holdqueue;

	State _state;
	int _substate;
	bool _awaiting_snapshot;
	bool _uploading_snapshot;

	int _streampointer;
	int _substreampointer;


	int _id;
	QString _username;
};

}

#endif
