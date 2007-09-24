/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2007 Calle Laakkonen

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
#ifndef NETWORK_H
#define NETWORK_H

#include <QMutex>
#include <QQueue>
#include <QTcpSocket>

namespace protocol {
	class Message;
}

//! Client network classes
/**
 */
namespace network {

//! Network connection handling thread
/**
 * Handles the reception and transmission of messages defined in the protocol.
 *
 * Messages can be sent using the \a send function.
 * The signal \a received is emitted when new messages have become available
 * and can be read with \a receive.
 */
class Connection : public QObject {
	Q_OBJECT
	public:
		Connection(QObject *parent=0);

		~Connection();

		//! Connect to a host
		void connectToHost(const QString& host, quint16 port);

		//! Disconnect
		void disconnectFromHost();

		//! Send a message
		void send(protocol::Message *msg);

		bool isConnected() const { return (socket.state() != QAbstractSocket::UnconnectedState); }

		//! Receive a message
		protocol::Message *receive();
		
		//! Get network error string
		QString errorString() const { return socket.errorString(); }
		
		void waitDisconnect();
	signals:
		//! Connection cut
		void disconnected();
		
		//! Connection established
		void connected();

		//! One or more message available in receive buffer
		void received();

		//! An error occured
		void error(const QString& message);

		//! Connection is about to disconnect
		void disconnecting();
		//! Start sending queued messages
		void sending();

	private slots:
		void socketError();
		
		//! Data has become available
		void dataAvailable();

		//! Send all pending messages from in queue
		void sendPending();

		//! Connection has been disconnected
		void hostDisconnected();
		//! Connection established
		void hostConnected();

		//! Network error has occured
		void networkError();

		//! Disconnect for real
		void disconnectHost();

	private:
		QTcpSocket socket;
		
		//const QString& host_;
		
		typedef QQueue<protocol::Message*> MessageQueue;
		
		// Sending
		QMutex sendmutex;
		MessageQueue sendqueue;
		char sendbuffer[1024];
		char *tmpbuffer; // tmpbuffer is used when sendbuffer is too short
		size_t sentlen, sendlen;

		// Receiving
		QMutex recvmutex;
		MessageQueue recvqueue;
		QByteArray recvbuffer;
		protocol::Message *newmsg;

		//! Serialize first message in send queue
		void serializeMessage();
};

}

#endif

