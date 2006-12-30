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

#include <QCoreApplication>
#include <QDebug>

#include "network.h"
#include "networkprivate.h"

#include "../shared/protocol.h"
#include "../shared/protocol.helper.h"

namespace network {

NetworkPrivate::NetworkPrivate(QObject *parent) :
	QObject(parent), sendbuffer(0), sentlen(0), sendlen(0), newmsg(0)
{
	// Indirect socket handling for thread safety
	connect(this, SIGNAL(sending()), this, SLOT(sendPending()),
			Qt::QueuedConnection);
	connect(this, SIGNAL(disconnecting()), this, SLOT(disconnectHost()),
			Qt::QueuedConnection);

	// Socket signals
	connect(&socket, SIGNAL(readyRead()), this, SLOT(dataAvailable()));
	connect(&socket, SIGNAL(disconnected()), this, SLOT(hostDisconnected()));
	connect(&socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(networkError()));
	connect(this, SIGNAL(sending()), this, SLOT(sendPending()));
	connect(this, SIGNAL(disconnecting()), this, SLOT(disconnectHost()));
}

NetworkPrivate::~NetworkPrivate()
{
	delete sendbuffer;
	delete newmsg;
}

/**
 * This function must be called from the network thread.
 * @param host hostname
 * @param port port
 * @return true on success
 */
bool NetworkPrivate::connectToHost(const QString& host, quint16 port)
{
	socket.connectToHost(host, port);
	return socket.waitForConnected();
}

/**
 * Calls internal disconnect function indirectly for thread safety.
 */
void NetworkPrivate::disconnectFromHost()
{
	emit disconnecting();
}

/**
 * Add a message to send queue. The message may contain a linked list, as long
 * as each message is of the same type. This function is thread safe.
 * @param msg message(s) to send
 */
void NetworkPrivate::send(protocol::Message *msg)
{
	Q_ASSERT(msg);
	sendmutex.lock();
	bool wasEmpty = sendqueue.isEmpty();
	// TODO aggregate subsequent messages of same type
	sendqueue.enqueue(msg);
	sendmutex.unlock();

	// If send queue was empty, notify network thread of new messages
	if(wasEmpty)
		emit sending();
}

/**
 * Get the oldest message from the receive queue.
 * @return received message (may be a linked list). 0 if no messages in queue.
 */
protocol::Message *NetworkPrivate::receive()
{
	recvmutex.lock();
	protocol::Message *msg;
	if(recvqueue.isEmpty())
		msg = 0;
	else
		msg = recvqueue.dequeue();
	recvmutex.unlock();
	return msg;
}

/**
 * Send all pending messages
 */
void NetworkPrivate::sendPending()
{
	do {
		if(sendlen==0)
			serializeMessage();
		// Loop until send queue is empty
		while(sendlen>0) {
			qint64 bytesleft = sendlen - sentlen;
			qint64 sent = socket.write(sendbuffer + sentlen, bytesleft);
			if(sent==-1) {
				// Error occured
			}
			if(sent == bytesleft) {
				delete sendbuffer;
				sendlen = 0;
				serializeMessage();
			} else {
				sentlen += sent;
			}
		}
		sendmutex.lock();
		bool isempty = sendqueue.isEmpty();
		sendmutex.unlock();
		if(isempty)
			break;
	} while(1);
}

/**
 * Take a message out of the queue and serialize it. If queue is empty,
 * sendbuffer and sendlen are set to zero.
 */
void NetworkPrivate::serializeMessage()
{
	Q_ASSERT(sendlen==0);
	sendmutex.lock();
	if(sendqueue.isEmpty()) {
		sendmutex.unlock();
		sendbuffer = 0;
	} else {
		protocol::Message *msg = sendqueue.dequeue();
		sendmutex.unlock();
		sendbuffer = msg->serialize(sendlen);
		delete msg;
	}
	sentlen = 0;
}

/**
 * Read data and unserialize available messages
 */
void NetworkPrivate::dataAvailable()
{
	// Read available data
	QByteArray buf = socket.read(99999);
	if(buf.length() == 0)
		return;
	recvbuffer.append(buf);

	// Try to unserialize as many messages as possible from buffer.
	// The buffer might not contain even a single complete message, so
	// state must be rememberd between calls.
	while(recvbuffer.length()>0) {
		// Identify packet
		if(newmsg==0) {
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
		if(reqlen <= recvbuffer.length()) {
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
			recvbuffer = recvbuffer.mid(reqlen);
		}
	}
}

/**
 * Private slot to handle disconnections
 */
void NetworkPrivate::hostDisconnected()
{
	emit disconnected(tr("Disconnected"));
}

/**
 * Private slot to handle network errors
 */
void NetworkPrivate::networkError()
{
	emit error(socket.errorString());
}

Connection::Connection(QObject *parent)
	: QThread(parent), p_(0)
{

}

Connection::~Connection()
{
	delete p_;
}

/**
 * Start the networking thread and connect to a host
 */
void Connection::connectHost(const QString& host, quint16 port)
{
	Q_ASSERT(isRunning() == false);

	host_ = host;
	port_ = port;
	start();
}

/**
 * Calls NetworkPrivate::disconnectHost
 */
void Connection::disconnectHost()
{
	Q_ASSERT(isRunning());
	p_->disconnectFromHost();
}

/**
 * Calls NetworkPrivate::send
 */
void Connection::send(protocol::Message *msg)
{
	Q_ASSERT(isRunning() && p_);
	p_->send(msg);
}

/**
 * Calls NetworkPrivate::receive
 */
protocol::Message *Connection::receive()
{
	Q_ASSERT(p_);
	return p_->receive();
}

//! Thread entry point
void Connection::run()
{
	qDebug() << "network thread starting";

	delete p_;
	p_ = new NetworkPrivate;

	// Relay signals
	connect(p_, SIGNAL(disconnected(QString)), this, SLOT(quit()));
	connect(p_, SIGNAL(disconnected(QString)), this, SIGNAL(disconnected(QString)));
	connect(p_, SIGNAL(received()), this, SIGNAL(received()));
	connect(p_, SIGNAL(error(QString)), this, SIGNAL(error(QString)));

	// Connect to host and enter event loop if succesfull
	if(p_->connectToHost(host_, port_)) {
		emit connected();
		exec();
	} else {
		emit disconnected(p_->errorString());
	}

	qDebug() << "network thread finished";
}

}

