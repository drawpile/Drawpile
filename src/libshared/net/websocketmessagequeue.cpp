// SPDX-License-Identifier: GPL-3.0-or-later
#include "libshared/net/websocketmessagequeue.h"
#include "libshared/util/qtcompat.h"
#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include <QWebSocket>
#include <QtEndian>

namespace net {

WebSocketMessageQueue::WebSocketMessageQueue(
	QWebSocket *socket, bool decodeOpaque, QObject *parent)
	: MessageQueue(decodeOpaque, parent)
	, m_socket(socket)
{
#ifdef __EMSCRIPTEN__
	// AutoConnection doesn't work here in Emscripten.
	Qt::ConnectionType connectionType = Qt::QueuedConnection;
#else
	Qt::ConnectionType connectionType = Qt::AutoConnection;
#endif
	connect(
		socket, &QWebSocket::binaryMessageReceived, this,
		&WebSocketMessageQueue::receiveBinaryMessage, connectionType);
	connect(
		socket, &QWebSocket::textMessageReceived, this,
		&WebSocketMessageQueue::receiveTextMessage, connectionType);
	connect(
		socket, &QWebSocket::bytesWritten, this,
		&WebSocketMessageQueue::dataWritten, connectionType);
}

int WebSocketMessageQueue::uploadQueueBytes() const
{
	return m_socket->bytesToWrite();
}

bool WebSocketMessageQueue::isUploading() const
{
	return uploadQueueBytes() != 0;
}

void WebSocketMessageQueue::enqueueMessages(int count, const net::Message *msgs)
{
	for(int i = 0; i < count; ++i) {
		if(msgs[i].serializeWs(m_serializationBuffer)) {
			qint64 sent = m_socket->sendBinaryMessage(m_serializationBuffer);
			if(sent != qint64(m_serializationBuffer.size())) {
				emit writeError();
				break;
			}
		} else {
			qWarning("Error serializing message: %s", DP_error());
		}
	}
}

void WebSocketMessageQueue::enqueuePing(bool pong)
{
	net::Message msg = net::makePingMessage(0, pong);
	enqueueMessages(1, &msg);
}

QAbstractSocket::SocketState WebSocketMessageQueue::getSocketState()
{
	return m_socket->state();
}

void WebSocketMessageQueue::abortSocket()
{
	m_socket->abort();
}

void WebSocketMessageQueue::dataWritten(qint64 bytes)
{
	emit bytesSent(bytes);
	if(m_socket->bytesToWrite() == 0) {
		emit allSent();
#ifndef __EMSCRIPTEN__
		if(m_gracefullyDisconnecting) {
			qInfo("All sent, gracefully disconnecting.");
			m_socket->close();
		}
#endif
	}
}

void WebSocketMessageQueue::receiveBinaryMessage(const QByteArray &bytes)
{
	// Ignore incoming messages while we're in the process of disconnecting.
	if(!m_gracefullyDisconnecting) {
		bool gotmessage = false;
		bool smoothFlush = false;
		int disconnectReason = -1;
		QString disconnectMessage;

		int type = static_cast<unsigned char>(bytes[0]);
		size_t messageLength = compat::cast<size_t>(bytes.size());
		if(type == MSG_TYPE_PING) {
			// Pings are handled internally
			if(messageLength != DP_MESSAGE_WS_HEADER_LENGTH + 1) {
				// Not a valid Ping message!
				emit badData(int(messageLength), MSG_TYPE_PING, 0);
			} else {
				handlePing(bytes[DP_MESSAGE_WS_HEADER_LENGTH]);
			}

		} else if(type == MSG_TYPE_DISCONNECT) {
			// Graceful disconnects are also handled internally
			if(messageLength < DP_MESSAGE_WS_HEADER_LENGTH + 1) {
				// We expected at least a reason!
				emit badData(int(messageLength), MSG_TYPE_DISCONNECT, 0);
			} else {
				smoothFlush = true;
				disconnectReason = bytes[DP_MESSAGE_WS_HEADER_LENGTH];
				disconnectMessage = QString::fromUtf8(
					bytes.constData() + DP_MESSAGE_WS_HEADER_LENGTH + 1,
					int(messageLength) - DP_MESSAGE_WS_HEADER_LENGTH - 1);
			}

		} else {
			// The rest are normal messages
			net::Message msg = net::Message::deserializeWs(
				reinterpret_cast<const unsigned char *>(bytes.constData()),
				messageLength, m_decodeOpaque);
			if(msg.isNull()) {
				qWarning("Error deserializing message: %s", DP_error());
				emit badData(
					int(messageLength), type,
					static_cast<unsigned char>(bytes[1]));
			} else {
				if(m_smoothTimer) {
					// Undos already have a delay because they require a
					// round trip, we don't want to make them even
					// slower.
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
				gotmessage = true;
			}
		}

		m_lastRecvTime = QDateTime::currentMSecsSinceEpoch();
		emit bytesReceived(compat::cast_6<int>(bytes.size()));

		if(gotmessage) {
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
}

void WebSocketMessageQueue::receiveTextMessage(const QString &text)
{
	qWarning("Unexpected WebSocket text message: '%s'", qUtf8Printable(text));
	emit readError();
}

void WebSocketMessageQueue::afterDisconnectSent()
{
	m_socket->flush();
#ifdef __EMSCRIPTEN__
	qInfo("Socket flushed, gracefully disconnecting.");
	m_socket->close(
		QWebSocketProtocol::CloseCodeNormal,
		QStringLiteral("gracefuldisconnect"));
#endif
}

}
