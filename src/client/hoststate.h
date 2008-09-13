/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2008 Calle Laakkonen

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
#ifndef HOSTSTATE_H
#define HOSTSTATE_H

#include <QTcpSocket>

#include "sessioninfo.h"

namespace protocol {
	class MessageQueue;
	class Message;
	class Packet;
};

namespace network {

class SessionState;

//! Network state machine
/**
 * This class handles the state of a host connection.
 */
class HostState : public QObject {
	Q_OBJECT
	friend class SessionState;
	public:
		//! Construct a HostState object
		HostState(QObject *parent);

		//! Get the ID of the local user
		const int localUser() const { return localuser_; }

		//! Connect to a host
		void connectToHost(const QString& host, quint16 port);

		//! Is a connection open?
		bool isConnected() { return socket_.state() != QAbstractSocket::UnconnectedState; }

		//! Get the session state
		SessionState *session() { return session_; }

		//! Initiate login sequence
		void login(const QString& username);

		//! Send a password
		void sendPassword(const QString& password);

		//! Send an arbitrary packet
		void sendPacket(const protocol::Packet& packet);

		//! Set options for hosting a session
		void setHost(const QString& password, const QString& title, quint16 width, quint16 height, int maxusers, bool allowDraw);

		//! Don't host a session
		void setNoHost();

	public slots:
		//! Disconnect from a host
		void disconnectFromHost();

	signals:
		//! A password must be requested from the user
		void needPassword();

		//! Login sequence completed succesfully
		void loggedin();

		//! The user is now a part of the session
		void joined();

		//! An error message was received from the host
		void error(const QString& message);

		//! Connection to host was established
		void connected();

		//! Connection was cut
		void disconnected();

	private slots:
		//! Do cleanups on disconnect
		void disconnectCleanup();

		//! Get a message from the network handler object
		void receiveMessage();

		//! Bad data was received
		void gotBadData();

		//! A network error occurred.
		void networkError();

	private:
		void handleMessage(const protocol::Message *msg);

		//! Set the 
		void sessionJoinDone(int localid);

		//! The local user id
		int localuser_;

		//! Name of the local user
		QString username_;

		//! Connection to the server
		QTcpSocket socket_;

		//! Message wrapper for the connection
		protocol::MessageQueue *mq_;

		//! Salt for generating password hash (received from the server)
		QString salt_;

		//! The active session
		SessionState *session_;

		//! Options for hosting a session
		Session host_;
		QString hostpassword_;
};

}

#endif

