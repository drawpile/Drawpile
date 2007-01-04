/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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

/*
 * Note. This header should only be included from network.cpp (and moc)
 */

#include <QMutex>
#include <QQueue>
#include <QTcpSocket>

namespace protocol {
	class Message;
}

namespace network {

typedef QQueue<protocol::Message*> MessageQueue;

//! Network private data
/**
 * This object lives in a separate thread and handles the actual
 * networking.
 */
class NetworkPrivate : public QObject {
	Q_OBJECT
	public:
		NetworkPrivate(QObject *parent=0);

		~NetworkPrivate();

		//! Connect to a host
		bool connectToHost(const QString& host, quint16 port);

		//! Disconnect
		void disconnectFromHost();

		//! Send a message
		void send(protocol::Message *msg);

		//! Receive a message
		protocol::Message *receive();

		//! Get network error string
		QString errorString() const { return socket.errorString(); }

	signals:
		//! Connection cut
		void disconnected(const QString& message);

		//! One or more message available in receive buffer
		void received();

		//! Send buffer emptied
		void sent();

		//! An error occured
		void error(const QString& message);

		//! Connection is about to disconnect
		void disconnecting();

		//! Start sending queued messages
		void sending();

	private slots:
		//! Data has become available
		void dataAvailable();

		//! Send all pending messages from in queue
		void sendPending();

		//! Connection has been disconnected
		void hostDisconnected();

		//! Network error has occured
		void networkError();

		//! Disconnect for real
		void disconnectHost() { socket.disconnectFromHost(); }

	private:
		QTcpSocket socket;

		// Sending
		QMutex sendmutex;
		MessageQueue sendqueue;
		char *sendbuffer;
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

