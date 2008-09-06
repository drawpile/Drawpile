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

#include <QCoreApplication>
#include <QDebug>

#include "network.h"

#if 0
#include "../shared/protocol.h"
#include "../shared/protocol.helper.h"
#endif

namespace network {

Connection::Connection(QObject *parent) :
	QObject(parent), tmpbuffer(0), sentlen(0), sendlen(0), newmsg(0)
{
	// Indirect socket handling for thread safety
	connect(this, SIGNAL(sending()), this, SLOT(sendPending()),
			Qt::QueuedConnection);
	connect(this, SIGNAL(disconnecting()), this, SLOT(disconnectHost()),
			Qt::QueuedConnection);

	// Socket signals
	connect(&socket, SIGNAL(readyRead()), this, SLOT(dataAvailable()));
	connect(&socket, SIGNAL(connected()), this, SLOT(hostConnected()));
	connect(&socket, SIGNAL(disconnected()), this, SLOT(hostDisconnected()));
	connect(&socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(networkError()));
}

Connection::~Connection()
{
	//delete [] tmpbuffer;
	//delete newmsg;
}

void Connection::disconnectHost()
{
	socket.disconnectFromHost();
}

/**
 * This function must be called from the network thread.
 * @param host hostname
 * @param port port
 * @return true on success
 */
void Connection::connectToHost(const QString& host, quint16 port)
{
	//host_ = host;
	socket.connectToHost(host, port);
}

/**
 * Calls internal disconnect function indirectly for thread safety.
 */
void Connection::disconnectFromHost()
{
	emit disconnecting();
}

#if 0
/**
 * Add a message to send queue. The message may contain a linked list, as long
 * as each message is of the same type. This function is thread safe.
 * @param msg message(s) to send
 */
void Connection::send(protocol::Message *msg)
{
	Q_ASSERT(msg);
	sendmutex.lock();
	bool wasEmpty = sendqueue.isEmpty();
#if 0 // Aggregate messages.
	if(wasempty==false) {
		protocol::Message *last = sendqueue.last();
		if(last->type == msg->type) {
			last->next = msg;
			msg->prev = last;
			sendqueue.last() = msg;
		} else {
			sendqueue.enqueue(msg);
		}
	} else
#endif
		sendqueue.enqueue(msg);
	sendmutex.unlock();

	// If send queue was empty, notify network thread of new messages
	if(wasEmpty)
		emit sending();
}
#endif

/**
 * Get the oldest message from the receive queue.
 * In case of a bundled message, a linked list is returned.
 * @return received message (may be a linked list). 0 if no messages in queue.
 */
protocol::Message *Connection::receive()
{
#if 0
	recvmutex.lock();
	protocol::Message *msg;
	if(recvqueue.isEmpty())
		msg = 0;
	else
		msg = recvqueue.dequeue();
	recvmutex.unlock();
	return msg;
#endif
	return 0;
}

/**
 * Send all pending messages
 */
void Connection::sendPending()
{
#if 0
	bool isempty;
	do {
		if(sendlen==0)
			serializeMessage();
		// Loop until send queue is empty
		while(sendlen>0) {
			qint64 bytesleft = sendlen - sentlen;
			qint64 sent = socket.write((tmpbuffer?tmpbuffer:sendbuffer) + sentlen, bytesleft);
			if(sent==-1) {
				// Error occured
				qDebug() << __FUNCTION__ << "sent==-1";
				return;
			}
			if(sent == bytesleft) {
				if(tmpbuffer) {
					delete [] tmpbuffer;
					tmpbuffer = 0;
				}
				sendlen = 0;
				serializeMessage();
			} else {
				sentlen += sent;
			}
		}
		sendmutex.lock();
		isempty = sendqueue.isEmpty();
		sendmutex.unlock();
	} while(!isempty);
#endif
}

/**
 * Take a message out of the queue and serialize it. If queue is empty,
 * sendbuffer and sendlen are set to zero.
 */
void Connection::serializeMessage()
{
#if 0
	Q_ASSERT(sendlen==0);
	sendmutex.lock();
	if(sendqueue.isEmpty()) {
		sendmutex.unlock();
	} else {
		protocol::Message *msg = sendqueue.dequeue();
		sendmutex.unlock();
		// Serialize the message. By default, try to use the preallocated
		// buffer. If it is not long enough, a new buffer is allocated
		// and returned.
		size_t tmp=sizeof(sendbuffer);
		char *buffer = msg->serialize(sendlen, sendbuffer, tmp);
		if(buffer != sendbuffer)
			tmpbuffer = buffer;
		delete msg;
	}
	sentlen = 0;
#endif
}

/**
 * Read data and unserialize available messages
 */
void Connection::dataAvailable()
{
#if 0
	do {
		// Read available data
		QByteArray buf = socket.readAll();
		if(buf.length() > 0)
			recvbuffer.append(buf);

		// Try to unserialize as many messages as possible from buffer.
		// The buffer might not contain even a single complete message, so
		// state must be rememberd between calls.
		// Identify packet
		if(newmsg==0) {
			if(recvbuffer.isEmpty()) {
				qDebug() << __FUNCTION__ << "newmsg==0 and recvbuffer is empty!";
				return;
			}
			newmsg = protocol::getMessage(recvbuffer[0]);
			if(newmsg==0) {
				// Unknown message received
				emit error(tr("Received unknown message type %1").arg(int(recvbuffer[0])));
				emit disconnecting();
				return;
			}
		}
		// Unserialize message if it has been received fully
		int reqlen = newmsg->reqDataLen(recvbuffer.constData(),
				recvbuffer.length());
		if(reqlen > recvbuffer.length()) 
			return ;
		newmsg->unserialize(recvbuffer.constData(), reqlen);
		
		// Enqueue the received message
		recvmutex.lock();
		bool wasEmpty = recvqueue.isEmpty();
		recvqueue.enqueue(newmsg);
		recvmutex.unlock();
		newmsg = 0;
		if(wasEmpty)
			emit received();

		// Remove unserialized bytes from receive buffer
		recvbuffer.remove(0,reqlen);
	} while(recvbuffer.length()>0);
#endif
}

/**
 * Private slot to handle disconnections
 */
void Connection::hostDisconnected()
{
	emit disconnected();
}

void Connection::hostConnected()
{
	emit connected();
}

/**
 * Private slot to handle network errors
 */
void Connection::networkError()
{
	qDebug() << "networkError:" << socket.errorString();
	emit error(socket.errorString());
}

void Connection::waitDisconnect()
{
	if (socket.state() != QAbstractSocket::UnconnectedState)
		socket.waitForDisconnected(-1);
}

void Connection::socketError()
{
	emit error(errorString());
}

}

