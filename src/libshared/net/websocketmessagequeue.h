// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBSHARED_NET_WEBSOCKETMESSSAGEQUEUE_H
#define LIBSHARED_NET_WEBSOCKETMESSSAGEQUEUE_H
#include "libshared/net/messagequeue.h"

class QWebSocket;

namespace net {

class WebSocketMessageQueue final : public MessageQueue {
	Q_OBJECT
public:
	WebSocketMessageQueue(
		QWebSocket *socket, bool decodeOpaque, QObject *parent = nullptr);

	int uploadQueueBytes() const override;
	bool isUploading() const override;

protected:
	void enqueueMessages(int count, const net::Message *msgs) override;
	void enqueuePing(bool pong) override;

	QAbstractSocket::SocketState getSocketState() override;
	void abortSocket() override;

private slots:
	void dataWritten(qint64 bytes);
	void receiveBinaryMessage(const QByteArray &bytes);
	void receiveTextMessage(const QString &text);

private:
	void afterDisconnectSent() override;

	QWebSocket *m_socket;
    QByteArray m_serializationBuffer;
};

}

#endif
