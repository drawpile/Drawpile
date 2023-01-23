/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2021 Calle Laakkonen

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

#include "libclient/net/messagequeue.h"
#include "libclient/net/envelopebuilder.h"
#include "rustpile/rustpile.h"

#include <QTcpSocket>
#include <QDateTime>
#include <QTimer>
#include <cstring>

namespace net {

// Reserve enough buffer space for one complete message
static const int MAX_BUF_LEN = 0xffff + Envelope::HEADER_LEN;

// Special message types handled internally by this class
static const int MSG_TYPE_DISCONNECT = 1;
static const int MSG_TYPE_PING = 2;

MessageQueue::MessageQueue(QTcpSocket *socket, QObject *parent)
	: QObject(parent), m_socket(socket),
	  m_pingTimer(nullptr),
	  m_lastRecvTime(0),
	  m_idleTimeout(0), m_pingSent(0),
	  m_gracefullyDisconnecting(false)
{
	connect(socket, &QTcpSocket::readyRead, this, &MessageQueue::readData);
	connect(socket, &QTcpSocket::bytesWritten, this, &MessageQueue::dataWritten);

	if(socket->inherits("QSslSocket")) {
		connect(socket, SIGNAL(encrypted()), this, SLOT(sslEncrypted()));
	}

	m_recvbuffer = new char[MAX_BUF_LEN];
	m_recvbytes = 0;
	m_sentbytes = 0;

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
	if(!m_gracefullyDisconnecting) {
		m_outbox.enqueue(message);
		if(m_sendbuffer.isEmpty())
			writeData();
	}
}

void MessageQueue::sendPingMsg(bool pong)
{
	EnvelopeBuilder eb;
	rustpile::write_ping(eb, 0, pong);

	send(eb.toEnvelope());
}

void MessageQueue::sendDisconnect(GracefulDisconnect reason, const QString &message)
{
	if(m_gracefullyDisconnecting)
		qWarning("sendDisconnect: already disconnecting.");

	EnvelopeBuilder eb;
	rustpile::write_disconnect(eb, 0, uint8_t(reason), reinterpret_cast<const uint16_t*>(message.constData()), message.length());

	qInfo("Sending disconnect message (reason=%d), will disconnect after queue (%lld messages) is empty.", int(reason), static_cast<long long>(m_outbox.size()));
	send(eb.toEnvelope());
	m_gracefullyDisconnecting = true;
	m_recvbytes = 0;
}

void MessageQueue::sendPing()
{
	if(m_pingSent==0) {
		m_pingSent = QDateTime::currentMSecsSinceEpoch();

	} else {
		// This can happen if the other side's upload buffer is too full
		// for the Pong to make it through in time.
		qWarning("sendPing(): reply to previous ping not yet received!");
	}

	sendPingMsg(false);

}

int MessageQueue::uploadQueueBytes() const
{
	int total = m_socket->bytesToWrite() + m_sendbuffer.length() - m_sentbytes;
	for(const Envelope &e: m_outbox)
		total += e.length();
	return total;
}

bool MessageQueue::isUploading() const
{
	return !m_sendbuffer.isEmpty() || m_socket->bytesToWrite() > 0;
}

qint64 MessageQueue::idleTime() const
{
	return QDateTime::currentMSecsSinceEpoch() - m_lastRecvTime;
}

void MessageQueue::readData() {
	bool gotmessage = false;
	int read, totalread=0;
	do {
		// Read as much as fits in to the message buffer
		read = m_socket->read(m_recvbuffer+m_recvbytes, MAX_BUF_LEN-m_recvbytes);
		if(read<0) {
			emit socketError(m_socket->errorString());
			return;
		}

		if(m_gracefullyDisconnecting) {
			// Ignore incoming data when we're in the process of disconnecting
			if(read>0)
				continue;
			else
				return;
		}

		m_recvbytes += read;

		// Extract all complete messages
		int messageLength;
		while(m_recvbytes >= Envelope::HEADER_LEN && m_recvbytes >= (messageLength=Envelope::sniffLength(m_recvbuffer))) {
			// Whole message received!

			if(Envelope::sniffType(m_recvbuffer) == MSG_TYPE_PING) {
				// Pings are handled internally
				if(messageLength != Envelope::HEADER_LEN + 1) {
					// Not a valid Ping message!
					emit badData(messageLength, MSG_TYPE_PING, 0);

				} else {
					handlePing(m_recvbuffer[Envelope::HEADER_LEN]);

				}

			} else if(Envelope::sniffType(m_recvbuffer) == MSG_TYPE_DISCONNECT) {
				// Graceful disconnects are also handled internally
				if(messageLength < Envelope::HEADER_LEN + 1) {
					// We expected at least a reason!
					emit badData(messageLength, MSG_TYPE_DISCONNECT, 0);

				} else {
					emit gracefulDisconnect(
						GracefulDisconnect(m_recvbuffer[Envelope::HEADER_LEN]),
							QString::fromUtf8(m_recvbuffer+Envelope::HEADER_LEN+1, messageLength - Envelope::HEADER_LEN - 1)
					);
				}

			} else {
				// The rest are normal messages
				m_inbox.append(m_recvbuffer, messageLength);
				gotmessage = true;
			}

			if(messageLength < m_recvbytes) {
				// Buffer contains more than one message
				memmove(m_recvbuffer, m_recvbuffer+messageLength, m_recvbytes-messageLength);
			}

			m_recvbytes -= messageLength;
		}

		// All whole messages extracted from the work buffer.
		// There can still be more bytes in the socket buffer.
		totalread += read;
	} while(read>0);

	if(totalread) {
		m_lastRecvTime = QDateTime::currentMSecsSinceEpoch();
		emit bytesReceived(totalread);
	}

	if(gotmessage)
		emit messageAvailable();
}

void MessageQueue::handlePing(bool isPong)
{
	if(isPong) {
		// We got a Pong back: measure latency
		if(m_pingSent==0) {
			// Lots of pings can have been queued up
			qDebug("Received Pong, but no Ping was sent!");

		} else {
			qint64 roundtrip = QDateTime::currentMSecsSinceEpoch() - m_pingSent;
			m_pingSent = 0;
			emit pingPong(roundtrip);
		}
	} else {
		// Reply to a Ping with a Pong
		sendPingMsg(true);
	}
}

void MessageQueue::dataWritten(qint64 bytes)
{
	emit bytesSent(bytes);

	// Write more once the buffer is empty
	if(m_socket->bytesToWrite()==0) {
		if(m_sendbuffer.isEmpty() && m_outbox.isEmpty() && m_gracefullyDisconnecting) {
			qInfo("All sent, gracefully disconnecting.");
			m_socket->disconnectFromHost();

		} else {
			writeData();
		}
	}
}

void MessageQueue::writeData() {
	bool sendMore = true;
	int sentBatch = 0;

	while(sendMore && sentBatch < 1024*64) {
		sendMore = false;
		if(m_sendbuffer.isEmpty() && !m_outbox.isEmpty()) {
			// Upload buffer is empty, but there are messages in the outbox
			Q_ASSERT(m_sentbytes == 0);

			auto msgpair = m_outbox.dequeue().breakApart();

			m_sendbuffer = msgpair.first;
			m_sentbytes = msgpair.second;
		}

		if(m_sentbytes < m_sendbuffer.length()) {
			const int sent = m_socket->write(m_sendbuffer.constData()+m_sentbytes, m_sendbuffer.length() - m_sentbytes);
			if(sent<0) {
				// Error
				emit socketError(m_socket->errorString());
				return;
			}
			m_sentbytes += sent;
			sentBatch += sent;

			Q_ASSERT(m_sentbytes <= m_sendbuffer.length());

			if(m_sentbytes >= m_sendbuffer.length()) {
				// Complete envelope sent
				m_sendbuffer = QByteArray();
				m_sentbytes = 0;
				sendMore = !m_outbox.isEmpty();
			}
		}
	}
}

}

