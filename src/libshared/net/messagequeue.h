// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_NET_MSGQUEUE_H
#define DP_NET_MSGQUEUE_H

#include "libshared/net/message.h"

#include <QQueue>
#include <QObject>

class QTcpSocket;
class QTimer;

namespace protocol {

/**
 * A wrapper for an IO device for sending and receiving messages.
 */
class MessageQueue final : public QObject {
Q_OBJECT
public:
	/**
	 * @brief Create a message queue that wraps a TCP socket.
	 *
	 * The MessageQueue does not take ownership of the device.
	 */
	explicit MessageQueue(QTcpSocket *socket, QObject *parent = nullptr);
	MessageQueue(const MessageQueue&) = delete;
	~MessageQueue() override;

	/**
	 * @brief Automatically decode opaque messages?
	 *
	 * This should be used on the client side only.
	 * @param d
	 */
	void setDecodeOpaque(bool d) { m_decodeOpaque = d; }

	/**
	 * @brief Check if there are new messages available
	 * @return true if getPending will return a message
	 */
	bool isPending() const;

	/**
	 * Get the next message in the queue.
	 * @return message
	 */
	MessagePtr getPending();

	/**
	 * Enqueue a message for sending.
	 */
	void send(const MessagePtr &message);
	void send(const MessageList &messages);

	/**
	 * @brief Gracefully disconnect
	 *
	 * This function enqueues the disconnect notification message. The connection will
	 * be automatically closed after the message has been sent. Additionally, it
	 * causes all incoming messages to be ignored.
	 *
	 * @param reason
	 * @param message
	 */
	void sendDisconnect(int reason, const QString &message);

	/**
	 * @brief Get the number of bytes in the upload queue
	 * @return
	 */
	int uploadQueueBytes() const;

	/**
	 * @brief Is there still data in the upload buffer?
	 */
	bool isUploading() const;

	/**
	 * @brief Get the number of milliseconds since the last message sent by the remote end
	 */
	qint64 idleTime() const;

	/**
	 * @brief Set the maximum time the remote end can be quiet before timing out
	 *
	 * This can be used together with a keepalive message to detect disconnects more
	 * reliably than relying on TCP, which may have a very long timeout.
	 *
	 * @param timeout timeout in milliseconds
	 */
	void setIdleTimeout(qint64 timeout);

	/**
	 * @brief Set Ping interval in milliseconds
	 *
	 * When ping interval is greater than zero, a Ping messages will automatically
	 * be sent.
	 *
	 * Note. This should be used by the client only.
	 *
	 * @param msecs
	 */
	void setPingInterval(int msecs);

#ifndef NDEBUG
	void setRandomLag(uint lag) { m_randomlag = lag; }
#endif

public slots:
	/**
	 * @brief Send a Ping message
	 *
	 * Note. Use this function instead of creating the Ping message yourself
	 * to make sure the roundtrip timer is set correctly!
	 * This function will not send another ping message until a reply has been received.
	 */
	void sendPing();

signals:
	/**
	 * @brief data reception statistics
	 * @param number of bytes received since last signal
	 */
	void bytesReceived(int count);

	/**
	 * @brief data transmission statistics
	 * @param count number of bytes sent since last signal
	 */
	void bytesSent(int count);

	/**
	 * New message(s) are available. Get them with getPending().
	 */
	void messageAvailable();

	/**
	 * All queued messages have been sent
	 */
	void allSent();

	/**
	 * An unrecognized message was received
	 * @param len length of the unrecognized message
	 * @param the unknown message identifier
	 * @param contextId the context ID of the message
	 */
	void badData(int len, int type, int contextId);

	void socketError(const QString &errorstring);

	/**
	 * @brief A reply to our Ping was just received
	 * @param roundtripTime time now - ping sent time
	 */
	void pingPong(qint64 roundtripTime);

private slots:
	void readData();
	void dataWritten(qint64);
	void sslEncrypted();
	void checkIdleTimeout();

private:
	void sendNow(MessagePtr msg);

	void writeData();

	QTcpSocket *m_socket;

	char *m_recvbuffer; // raw message reception buffer
	char *m_sendbuffer; // raw message upload buffer
	int m_recvbytes;    // number of bytes in reception buffer
	int m_sentbytes;    // number of bytes in upload buffer already sent
	int m_sendbuflen;   // length of the data in the upload buffer

	QQueue<MessagePtr> m_inbox;  // pending messages
	QQueue<MessagePtr> m_outbox; // messages to be sent

	QTimer *m_idleTimer;
	QTimer *m_pingTimer;
	qint64 m_lastRecvTime;
	qint64 m_idleTimeout;
	qint64 m_pingSent;

	bool m_closeWhenReady;
	bool m_ignoreIncoming;

	bool m_decodeOpaque;

#ifndef NDEBUG
	uint m_randomlag;
#endif
};

}

#endif

