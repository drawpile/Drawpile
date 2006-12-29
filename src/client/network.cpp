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
#include <QTcpSocket>
#include <QQueue>
#include <QMutex>

#include "network.h"
#include "../shared/protocol.h"
#include "../shared/protocol.helper.h"

typedef QQueue<protocol::Message*> MessageQueue;

static const int SendAvailableEvent = QEvent::User + 1;
static const int DisconnectEvent = QEvent::User + 2;

//! Network private data
class NetworkPrivate {
	public:
		NetworkPrivate() : sendbuffer(0), sentlen(0), sendlen(0), newmsg(0) {}

		~NetworkPrivate()
		{
			delete sendbuffer;
			delete newmsg;
		}

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

		//! Send all pending messages
		void sendMessages();
		//! Serialize first message in send queue
		void serializeMessage();
};

Network::Network(QObject *parent)
	: QThread(parent), p_(0)
{

}

Network::~Network()
{
	delete p_;
}

void Network::connectHost(const QString& host, quint16 port)
{
	Q_ASSERT(isRunning() == false);

	host_ = host;
	port_ = port;
	start();
}

void Network::disconnectHost()
{
	Q_ASSERT(isRunning());
	QCoreApplication::postEvent(this, new QEvent(QEvent::Type(DisconnectEvent)));
}

/**
 * Add a message to send queue. The message may contain a linked list, as long
 * as each message is of the same type.
 * @param msg message(s) to send
 */
void Network::send(protocol::Message *msg)
{
	Q_ASSERT(p_ && msg);
	p_->sendmutex.lock();
	bool wasEmpty = p_->sendqueue.isEmpty();
	// TODO aggregate subsequent messages of same type
	p_->sendqueue.enqueue(msg);
	p_->sendmutex.unlock();

	// If send queue was empty, notify network thread of new messages
	if(wasEmpty)
		QCoreApplication::postEvent(this, new QEvent(QEvent::Type(SendAvailableEvent)));
}

/**
 * Get the oldest message from the receive queue.
 * @return received message (may be a linked list). 0 if no messages in queue.
 */
protocol::Message *Network::receive()
{
	Q_ASSERT(p_);
	p_->recvmutex.lock();
	protocol::Message *msg;
	if(p_->recvqueue.isEmpty())
		msg = 0;
	else
		msg = p_->recvqueue.dequeue();
	p_->recvmutex.unlock();
	return msg;
}

/**
 * Handle commands from the main thread
 * @param event event info
 */
void Network::customEvent(QEvent *event)
{
	if(event->type() == SendAvailableEvent)
		p_->sendMessages();
	else if(event->type() == DisconnectEvent)
		p_->socket.disconnectFromHost();
}

/**
 * Loops until sendqueue is empty
 */
void NetworkPrivate::sendMessages()
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
		sendlen = 0;
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
void Network::dataAvailable()
{
	// Read available data
	QByteArray buf = p_->socket.read(99999);
	if(buf.length() == 0)
		return;
	p_->recvbuffer.append(buf);

	// Try to unserialize as many messages as possible from buffer.
	// The buffer might not contain even a single complete message, so
	// state must be rememberd between calls.
	while(p_->recvbuffer.length()>0) {
		// Identify packet
		if(p_->newmsg==0) {
			p_->newmsg = protocol::stack::get(p_->recvbuffer[0]);
		}
		// Unserialize message if it has been received fully
		int reqlen = p_->newmsg->reqDataLen(p_->recvbuffer.constData(),
				p_->recvbuffer.length());
		if(reqlen <= p_->recvbuffer.length()) {
			p_->newmsg->unserialize(p_->recvbuffer.constData(), reqlen);
			
			// Enqueue the received message
			p_->recvmutex.lock();
			p_->recvqueue.enqueue(p_->newmsg);
			p_->recvmutex.unlock();
			p_->newmsg = 0;

			// Remove unserialized bytes from receive buffer
			p_->recvbuffer = p_->recvbuffer.mid(reqlen);
		}
	}
}

/**
 * Private slot to handle disconnections
 */
void Network::hostDisconnected()
{
	emit disconnected(tr("Disconnected"));
	quit();
}

/**
 * Private slot to handle network errors
 */
void Network::networkError()
{
	emit error(p_->socket.errorString());
}

//! Thread entry point
void Network::run()
{
	delete p_;
	p_ = new NetworkPrivate;

	qDebug() << "network thread starting";

	// Connect to host
	p_->socket.connectToHost(host_, port_);
	if (!p_->socket.waitForConnected()) {
		// Connection failed
		emit disconnected(p_->socket.errorString());
	} else {
		// Connect signals and slots.
		// Note that we must explicitly use direct connections, as the slots
		// are only used from inside this thread and modify private data
		// directly.
		connect(&p_->socket, SIGNAL(readyRead()),
				this, SLOT(dataAvailable()), Qt::DirectConnection);
		connect(&p_->socket, SIGNAL(disconnected()),
				this, SLOT(hostDisconnected()), Qt::DirectConnection);
		connect(&p_->socket, SIGNAL(error(QAbstractSocket::SocketError)),
				this, SLOT(networkError()), Qt::DirectConnection);

		emit connected();

		// Enter thread event loop
		exec();
	}
	qDebug() << "network thread finished";
}

