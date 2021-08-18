/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "messagequeue.h"
#include "../../libshared/net/control.h"

#include <QTcpSocket>
#include <QDateTime>
#include <QTimer>
#include <cstring>

namespace net {

// Reserve enough buffer space for one complete message
static const int MAX_BUF_LEN = 0xffff + protocol::Message::HEADER_LEN;

MessageQueue::MessageQueue(QTcpSocket *socket, QObject *parent)
	: QObject(parent), m_socket(socket),
	  m_pingTimer(nullptr),
	  m_lastRecvTime(0),
	  m_idleTimeout(0), m_pingSent(0), m_closeWhenReady(false),
	  m_ignoreIncoming(false)
{
	connect(socket, &QTcpSocket::readyRead, this, &MessageQueue::readData);
	connect(socket, &QTcpSocket::bytesWritten, this, &MessageQueue::dataWritten);

	if(socket->inherits("QSslSocket")) {
		connect(socket, SIGNAL(encrypted()), this, SLOT(sslEncrypted()));
	}

	m_recvbuffer = new char[MAX_BUF_LEN];
	m_sendbuffer = new char[MAX_BUF_LEN];
	m_recvbytes = 0;
	m_sentbytes = 0;
	m_sendbuflen = 0;

	m_idleTimer = new QTimer(this);
	m_idleTimer->setTimerType(Qt::CoarseTimer);
	connect(m_idleTimer, &QTimer::timeout, this, &MessageQueue::checkIdleTimeout);
	m_idleTimer->setInterval(1000);
	m_idleTimer->setSingleShot(false);
}

void MessageQueue::sslEncrypted()
{
	disconnect(m_socket, &QTcpSocket::bytesWritten, this, &MessageQueue::dataWritten);
	connect(m_socket, SIGNAL(encryptedBytesWritten(qint64)), this, SLOT(dataWritten(qint64)));
}

void MessageQueue::checkIdleTimeout()
{
	if(m_idleTimeout>0 && m_socket->state() == QTcpSocket::ConnectedState && idleTime() > m_idleTimeout) {
		qWarning("MessageQueue timeout");
		m_socket->abort();
	}
}

void MessageQueue::setIdleTimeout(qint64 timeout)
{
	m_idleTimeout = timeout;
	m_lastRecvTime = QDateTime::currentMSecsSinceEpoch();
	if(timeout>0)
		m_idleTimer->start(1000);
	else
		m_idleTimer->stop();
}

void MessageQueue::setPingInterval(int msecs)
{
	if(!m_pingTimer) {
		m_pingTimer = new QTimer(this);
		m_pingTimer->setTimerType(Qt::CoarseTimer);
		m_pingTimer->setSingleShot(false);
		connect(m_pingTimer, &QTimer::timeout, this, &MessageQueue::sendPing);
	}
	m_pingTimer->setInterval(msecs);
	m_pingTimer->start(msecs);
}

MessageQueue::~MessageQueue()
{
	delete [] m_recvbuffer;
	delete [] m_sendbuffer;
}

bool MessageQueue::isPending() const
{
	return !m_inbox.isEmpty();
}

Envelope MessageQueue::getPending()
{
	Envelope e = m_inbox;
	m_inbox = Envelope();
	return e;
}

void MessageQueue::send(const Envelope &message)
{
	if(!m_closeWhenReady) {
		m_outbox.enqueue(message);
		if(m_sendbuflen==0)
			writeData();
	}
}

void MessageQueue::sendNow(const Envelope &message)
{
	if(!m_closeWhenReady) {
		m_outbox.prepend(message);
		if(m_sendbuflen==0)
			writeData();
	}
}

void MessageQueue::sendDisconnect(int reason, const QString &message)
{
	protocol::Disconnect msg(0, protocol::Disconnect::Reason(reason), message);
	QByteArray b(msg.length(), 0); // FIXME do this better
	msg.serialize(b.data());

	send(b);
	m_ignoreIncoming = true;
	m_recvbytes = 0;
}

void MessageQueue::sendPing()
{
	if(m_pingSent==0) {
		m_pingSent = QDateTime::currentMSecsSinceEpoch();
	} else {
		// This shouldn't happen, but we'll resend a ping anyway just to be safe.
		qWarning("sendPing(): reply to previous ping not yet received!");
	}

	protocol::Ping msg(0, false);
	QByteArray b(msg.length(), 0);
	msg.serialize(b.data());

	sendNow(b);
}

int MessageQueue::uploadQueueBytes() const
{
	int total = m_socket->bytesToWrite() + m_sendbuflen - m_sentbytes;
	for(const Envelope &e: m_outbox)
		total += e.length();
	return total;
}

bool MessageQueue::isUploading() const
{
	return m_sendbuflen > 0 || m_socket->bytesToWrite() > 0;
}

qint64 MessageQueue::idleTime() const
{
	return QDateTime::currentMSecsSinceEpoch() - m_lastRecvTime;
}

void MessageQueue::readData() {
	bool gotmessage = false;
	int read, totalread=0;
	do {
		// Read as much as fits in to the deserialization buffer
		read = m_socket->read(m_recvbuffer+m_recvbytes, MAX_BUF_LEN-m_recvbytes);
		if(read<0) {
			emit socketError(m_socket->errorString());
			return;
		}

		if(m_ignoreIncoming) {
			// Ignore incoming data mode is used when we're shutting down the connection
			// but want to clear the upload queue
			if(read>0)
				continue;
			else
				return;
		}

		m_recvbytes += read;

		// Extract all complete messages
		int len;
		while(m_recvbytes >= protocol::Message::HEADER_LEN && m_recvbytes >= (len=protocol::Message::sniffLength(m_recvbuffer))) {
			// Whole message received!
			Envelope envelope((const uchar*)m_recvbuffer, len);

			if(envelope.messageType() == protocol::MSG_PING) {
				// Special handing for Ping messages
				protocol::NullableMessageRef msg = protocol::Message::deserialize(envelope.data(), envelope.length(), true);
				Q_ASSERT(!msg.isNull()); // FIXME don't use the old deserializer
				Q_ASSERT(msg->type() == protocol::MSG_PING);
				bool isPong = msg.cast<protocol::Ping>().isPong();

				if(isPong) {
					if(m_pingSent==0) {
						qWarning("Received Pong, but no Ping was sent!");

					} else {
						qint64 roundtrip = QDateTime::currentMSecsSinceEpoch() - m_pingSent;
						m_pingSent = 0;
						emit pingPong(roundtrip);
					}
				} else {
					protocol::Ping msg(0, true);
					QByteArray b(msg.length(), 0);
					msg.serialize(b.data());

					sendNow(b);
				}
			} else {
				m_inbox.append(envelope);
				gotmessage = true;
			}

			if(len < m_recvbytes) {
				// Buffer contains more than one message
				memmove(m_recvbuffer, m_recvbuffer+len, m_recvbytes-len);
			}
			m_recvbytes -= len;
		}

		// All messages extracted from buffer (if there were any):
		// see if there are more bytes in the socket buffer
		totalread += read;
	} while(read>0);

	if(totalread) {
		m_lastRecvTime = QDateTime::currentMSecsSinceEpoch();
		emit bytesReceived(totalread);
	}

	if(gotmessage)
		emit messageAvailable();
}

void MessageQueue::dataWritten(qint64 bytes)
{
	emit bytesSent(bytes);

	// Write more once the buffer is empty
	if(m_socket->bytesToWrite()==0) {
		if(m_sendbuflen==0 && m_outbox.isEmpty())
			emit allSent();
		else
			writeData();
	}
}

void MessageQueue::writeData() {
	int sentBatch = 0;
	bool sendMore = true;

	while(sendMore && sentBatch < 1024*64) {
		sendMore = false;
		if(m_sendbuflen==0 && !m_outbox.isEmpty()) {
			// Upload buffer is empty, but there are messages in the outbox
			Q_ASSERT(m_sentbytes == 0);

			// TODO we could just use the envelope instead of the sendbuffer
			Envelope msg = m_outbox.dequeue();
			memcpy(m_sendbuffer, msg.data(), msg.length());
			m_sendbuflen = msg.length();
			Q_ASSERT(m_sendbuflen>0);
			Q_ASSERT(m_sendbuflen <= MAX_BUF_LEN);

			if(msg.messageType() == protocol::MSG_DISCONNECT) {
				// Automatically disconnect after Disconnect notification is sent
				m_closeWhenReady = true;
				m_outbox.clear();
			}

			// An envelope can contain more than one message
			msg = msg.next();
			if(!msg.isEmpty())
				m_outbox.prepend(msg);
		}

		if(m_sentbytes < m_sendbuflen) {
			const int sent = m_socket->write(m_sendbuffer+m_sentbytes, m_sendbuflen-m_sentbytes);
			if(sent<0) {
				// Error
				emit socketError(m_socket->errorString());
				return;
			}
			m_sentbytes += sent;
			sentBatch += sent;

			Q_ASSERT(m_sentbytes <= m_sendbuflen);
			if(m_sentbytes >= m_sendbuflen) {
				// Complete message sent
				m_sendbuflen=0;
				m_sentbytes=0;
				if(m_closeWhenReady) {
					m_socket->disconnectFromHost();

				} else {
					sendMore = true;
				}
			}
		}
	}
}

}

