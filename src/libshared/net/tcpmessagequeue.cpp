// SPDX-License-Identifier: GPL-3.0-or-later
#include "libshared/net/tcpmessagequeue.h"
#include "libshared/util/qtcompat.h"
#include <QDateTime>
#include <QTcpSocket>
#include <QTimer>
#include <QtEndian>

namespace net {

TcpMessageQueue::TcpMessageQueue(
	QTcpSocket *socket, bool decodeOpaque, QObject *parent)
	: MessageQueue(decodeOpaque, parent)
	, m_socket(socket)
{
	m_recvbuffer = new char[MAX_BUF_LEN];
	m_recvbytes = 0;
	m_sentbytes = 0;

	connect(socket, &QTcpSocket::readyRead, this, &TcpMessageQueue::readData);
	connect(
		socket, &QTcpSocket::bytesWritten, this, &TcpMessageQueue::dataWritten);

	if(socket->inherits("QSslSocket")) {
		connect(socket, SIGNAL(encrypted()), this, SLOT(sslEncrypted()));
	}
}

TcpMessageQueue::~TcpMessageQueue()
{
	delete[] m_recvbuffer;
}

int TcpMessageQueue::uploadQueueBytes() const
{
	int total = m_socket->bytesToWrite() + m_sendbuffer.length() - m_sentbytes;
	for(const net::Message &msg : m_outbox) {
		total += compat::castSize(msg.length());
	}
	total +=
		m_pings.size() * (DP_MESSAGE_HEADER_LENGTH + DP_MSG_PING_STATIC_LENGTH);
	return total;
}

bool TcpMessageQueue::isUploading() const
{
	return !m_sendbuffer.isEmpty() || m_socket->bytesToWrite() > 0;
}

void TcpMessageQueue::enqueueMessages(int count, const net::Message *msgs)
{
	for(int i = 0; i < count; ++i) {
		m_outbox.enqueue(msgs[i]);
	}
	if(m_sendbuffer.isEmpty()) {
		writeData();
	}
}

void TcpMessageQueue::enqueuePing(bool pong)
{
	m_pings.enqueue(pong);
	if(m_sendbuffer.isEmpty()) {
		writeData();
	}
}

QAbstractSocket::SocketState TcpMessageQueue::getSocketState()
{
	return m_socket->state();
}

void TcpMessageQueue::abortSocket()
{
	m_socket->abort();
}

void TcpMessageQueue::readData()
{
	int read, totalread = 0, gotmessages = 0;
	bool smoothFlush = false;
	int disconnectReason = -1;
	QString disconnectMessage;
	do {
		// Read as much as fits in to the message buffer
		read = m_socket->read(
			m_recvbuffer + m_recvbytes, MAX_BUF_LEN - m_recvbytes);
		if(read < 0) {
			emit readError();
			return;
		}

		if(m_gracefullyDisconnecting && !m_quietDisconnecting) {
			// Ignore incoming data when we're in the process of disconnecting
			if(read > 0) {
				continue;
			} else {
				return;
			}
		}

		m_recvbytes += read;

		// Extract all complete messages
		int messageLength;
		while((messageLength = haveWholeMessageToRead()) != 0) {
			// Whole message received!
			int type = static_cast<unsigned char>(m_recvbuffer[2]);
			if(type == MSG_TYPE_PING) {
				// Pings are handled internally
				if(messageLength != DP_MESSAGE_HEADER_LENGTH + 1) {
					// Not a valid Ping message!
					emit badData(messageLength, MSG_TYPE_PING, 0);
				} else {
					handlePing(m_recvbuffer[DP_MESSAGE_HEADER_LENGTH]);
				}

			} else if(type == MSG_TYPE_DISCONNECT) {
				// Graceful disconnects are also handled internally
				if(messageLength < DP_MESSAGE_HEADER_LENGTH + 1) {
					// We expected at least a reason!
					emit badData(messageLength, MSG_TYPE_DISCONNECT, 0);
				} else {
					smoothFlush = true;
					disconnectReason = m_recvbuffer[DP_MESSAGE_HEADER_LENGTH];
					disconnectMessage = QString::fromUtf8(
						m_recvbuffer + DP_MESSAGE_HEADER_LENGTH + 1,
						messageLength - DP_MESSAGE_HEADER_LENGTH - 1);
				}

			} else if(type == MSG_TYPE_KEEP_ALIVE) {
				// Nothing to do, just keeps the connection alive if the client
				// fails to send out a ping due upload queue saturation.

			} else if(m_gracefullyDisconnecting) {
				// Just keep echoing everything that has a client effect.
				if(type == MSG_TYPE_CHAT || type == MSG_TYPE_PRIVATE_CHAT ||
				   type >= MSG_TYPE_CLIENT_META) {
					m_outbox.enqueue(Message::noinc(DP_message_new_opaque(
						DP_MessageType(type), m_recvbuffer[3],
						reinterpret_cast<const unsigned char *>(
							m_recvbuffer + DP_MESSAGE_HEADER_LENGTH),
						messageLength - DP_MESSAGE_HEADER_LENGTH)));
					if(m_sendbuffer.isEmpty()) {
						writeData();
					}
				}

			} else {
				// The rest are normal messages
				net::Message msg = net::Message::deserialize(
					reinterpret_cast<unsigned char *>(m_recvbuffer),
					m_recvbytes, m_decodeOpaque);
				if(msg.isNull()) {
					qWarning("Error deserializing message: %s", DP_error());
					emit badData(
						messageLength, type,
						static_cast<unsigned char>(m_recvbuffer[3]));
				} else {
					if(m_smoothTimer) {
						// Undos already have a delay because they require a
						// round trip, we don't want to make them even slower.
						bool ownUndoReceived = m_contextId != 0 &&
											   msg.type() == DP_MSG_UNDO &&
											   msg.contextId() == m_contextId;
						if(ownUndoReceived) {
							smoothFlush = true;
						}
						m_smoothBuffer.append(msg);
					} else {
						m_inbox.append(msg);
					}
					++gotmessages;
				}
			}

			if(messageLength < m_recvbytes) {
				// Buffer contains more than one message
				memmove(
					m_recvbuffer, m_recvbuffer + messageLength,
					m_recvbytes - messageLength);
			}

			m_recvbytes -= messageLength;
		}

		// All whole messages extracted from the work buffer.
		// There can still be more bytes in the socket buffer.
		totalread += read;

		// Only read up to 1000 or 10 MiB worth of messages in one go to not
		// delay other processes too much.
		if(gotmessages >= 1000 || totalread >= 10 * 1024 * 1024) {
			QTimer::singleShot(0, this, &TcpMessageQueue::readData);
			break;
		}
	} while(read > 0);

	if(totalread != 0) {
		resetLastRecvTimer();
		emit bytesReceived(totalread);
	}

	if(gotmessages != 0) {
		if(m_smoothTimer) {
			if(smoothFlush) {
				m_inbox.append(m_smoothBuffer);
				m_smoothBuffer.clear();
				emit messageAvailable();
				m_smoothTimer->stop();
			} else {
				m_smoothMessagesToDrain =
					m_smoothBuffer.size() / m_smoothDrainRate;
				if(!m_smoothTimer->isActive()) {
					receiveSmoothedMessages();
				}
			}
		} else {
			emit messageAvailable();
		}
	}

	if(disconnectReason != -1) {
		emit gracefulDisconnect(
			GracefulDisconnect(disconnectReason), disconnectMessage);
	}
}

void TcpMessageQueue::dataWritten(qint64 bytes)
{
	emit bytesSent(bytes);

	// Write more once the buffer is empty
	if(m_socket->bytesToWrite() == 0) {
		if(m_sendbuffer.isEmpty() && !messagesInOutbox()) {
			emit allSent();
			if(m_gracefullyDisconnecting && !m_quietDisconnecting) {
				qInfo("All sent, gracefully disconnecting.");
				m_socket->disconnectFromHost();
			}
		} else {
			writeData();
		}
	}
}

void TcpMessageQueue::sslEncrypted()
{
	disconnect(
		m_socket, &QTcpSocket::bytesWritten, this,
		&TcpMessageQueue::dataWritten);
	connect(
		m_socket, SIGNAL(encryptedBytesWritten(qint64)), this,
		SLOT(dataWritten(qint64)));
}

void TcpMessageQueue::afterDisconnectSent()
{
	m_recvbytes = 0;
}

int TcpMessageQueue::haveWholeMessageToRead()
{
	if(m_recvbytes >= DP_MESSAGE_HEADER_LENGTH) {
		int bodyLength = qFromBigEndian<quint16>(m_recvbuffer);
		int messageLength = bodyLength + DP_MESSAGE_HEADER_LENGTH;
		if(m_recvbytes >= messageLength) {
			return messageLength;
		}
	}
	return 0;
}

void TcpMessageQueue::writeData()
{
	bool sendMore = true;
	int sentBatch = 0;

	while(sendMore && sentBatch < 1024 * 64) {
		sendMore = false;
		if(m_sendbuffer.isEmpty() && messagesInOutbox()) {
			// Upload buffer is empty, but there are messages in the outbox
			Q_ASSERT(m_sentbytes == 0);
			net::Message msg = dequeueFromOutbox();
			if(msg.isNull()) {
				continue;
			} else if(!msg.serialize(m_sendbuffer)) {
				qWarning("Error serializing message: %s", DP_error());
				sendMore = messagesInOutbox();
				continue;
			}
		}

		if(m_sentbytes < m_sendbuffer.length()) {
			const int sent = m_socket->write(
				m_sendbuffer.constData() + m_sentbytes,
				m_sendbuffer.length() - m_sentbytes);
			if(sent < 0) {
				emit writeError();
				return;
			}
			m_sentbytes += sent;
			sentBatch += sent;

			Q_ASSERT(m_sentbytes <= m_sendbuffer.length());

			if(m_sentbytes >= m_sendbuffer.length()) {
				// Complete envelope sent
				m_sendbuffer.clear();
				m_sentbytes = 0;
				sendMore = messagesInOutbox();
			}
		}
	}
}

bool TcpMessageQueue::messagesInOutbox() const
{
	return !m_outbox.isEmpty() || !m_pings.isEmpty();
}

net::Message TcpMessageQueue::dequeueFromOutbox()
{
	if(m_pings.isEmpty()) {
		return m_outbox.dequeue();
	} else {
		return net::makePingMessage(0, m_pings.dequeue());
	}
}

}
