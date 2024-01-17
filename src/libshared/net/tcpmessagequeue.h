// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBSHARED_NET_TCPMESSSAGEQUEUE_H
#define LIBSHARED_NET_TCPMESSSAGEQUEUE_H
#include "libshared/net/messagequeue.h"
#include <QQueue>

class QTcpSocket;

namespace net {

class TcpMessageQueue final : public MessageQueue {
	Q_OBJECT
public:
	TcpMessageQueue(
		QTcpSocket *socket, bool decodeOpaque, QObject *parent = nullptr);

	~TcpMessageQueue() override;

	int uploadQueueBytes() const override;
	bool isUploading() const override;

protected:
	void enqueueMessages(int count, const net::Message *msgs) override;
	void enqueuePing(bool pong) override;

	QAbstractSocket::SocketState getSocketState() override;
	void abortSocket() override;

private slots:
	void readData();
	void dataWritten(qint64 bytes);
	void sslEncrypted();

private:
	static constexpr int MAX_BUF_LEN = 0xffff + DP_MESSAGE_HEADER_LENGTH;

	void afterDisconnectSent() override;

	int haveWholeMessageToRead();

	void writeData();

	bool messagesInOutbox() const;
	net::Message dequeueFromOutbox();

	QTcpSocket *m_socket;
	char *m_recvbuffer;		 // raw message reception buffer
	QByteArray m_sendbuffer; // raw message upload buffer
	int m_recvbytes;		 // number of bytes in reception buffer
	int m_sentbytes;		 // number of bytes in upload buffer already sent
	QQueue<net::Message> m_outbox; // messages to be sent
	QQueue<bool> m_pings;		   // pings and pongs to be sent
};

}

#endif
