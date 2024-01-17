// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBSHARED_NET_MESSAGEQUEUE_H
#define LIBSHARED_NET_MESSAGEQUEUE_H
#include "libshared/net/message.h"
#include <QAbstractSocket>
#include <QObject>
#include <QVector>

class QTimer;

namespace net {

/**
 * A wrapper for an IO device for sending and receiving messages.
 */
class MessageQueue : public QObject {
	Q_OBJECT
public:
	static constexpr int DEFAULT_SMOOTH_DRAIN_RATE = 20;
	static constexpr int MAX_SMOOTH_DRAIN_RATE = 60;

	enum class GracefulDisconnect {
		Error,	  // An error occurred
		Kick,	  // client was kicked by the session operator
		Shutdown, // server is shutting down
		Other,	  // other unspecified error
	};

	MessageQueue(bool decodeOpaque, QObject *parent);

	/**
	 * @brief Check if there are new messages available
	 * @return true if getPending will return a message
	 */
	bool isPending() const;

	net::Message shiftPending();

	/**
	 * Get received messages, swaps (!) the given buffer with the inbox.
	 */
	void receive(net::MessageList &buffer);

	/**
	 * Enqueue a single message for sending.
	 */
	void send(const net::Message &msg);

	/**
	 * Enqueue multiple messages for sending.
	 */
	void sendMultiple(int count, const net::Message *msgs);

	/**
	 * @brief Gracefully disconnect
	 *
	 * This function enqueues the disconnect notification message. The
	 * connection will be automatically closed after the message has been sent.
	 * Additionally, it causes all incoming messages to be ignored and no more
	 * data to be accepted for sending.
	 *
	 * @param reason
	 * @param message
	 */
	void sendDisconnect(GracefulDisconnect reason, const QString &message);

	/**
	 * @brief Get the number of bytes in the upload queue
	 * @return
	 */
	virtual int uploadQueueBytes() const = 0;

	/**
	 * @brief Is there still data in the upload buffer?
	 */
	virtual bool isUploading() const = 0;

	/**
	 * @brief Get the number of milliseconds since the last message sent by the
	 * remote end
	 */
	qint64 idleTime() const;

	/**
	 * @brief Set the maximum time the remote end can be quiet before timing out
	 *
	 * This can be used together with a keepalive message to detect disconnects
	 * more reliably than relying on TCP, which may have a very long timeout.
	 *
	 * @param timeout timeout in milliseconds
	 */
	void setIdleTimeout(qint64 timeout);

	/**
	 * @brief Set Ping interval in milliseconds
	 *
	 * When ping interval is greater than zero, a Ping messages will
	 * automatically be sent.
	 *
	 * Note. This should be used by the client only.
	 *
	 * @param msecs
	 */
	void setPingInterval(int msecs);

	void setSmoothEnabled(bool smoothingEnabled);
	void setSmoothDrainRate(int smoothDrainRate);

	int artificalLagMs() { return m_artificialLagMs; }

	void setArtificialLagMs(int msecs);

	void setContextId(unsigned int contextId) { m_contextId = contextId; }

public slots:
	/**
	 * @brief Send a Ping message
	 *
	 * Note. Use this function instead of creating the Ping message yourself
	 * to make sure the roundtrip timer is set correctly!
	 * This function will not send another ping message until a reply has been
	 * received.
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
	 * @param roundtripTime milliseconds since we sent our ping
	 */
	void pingPong(qint64 roundtripTime);

	/**
	 * The server sent a graceful disconnect notification
	 */
	void gracefulDisconnect(GracefulDisconnect reason, const QString &message);

protected slots:
	void receiveSmoothedMessages();

protected:
	static constexpr int MSG_TYPE_DISCONNECT = 1;
	static constexpr int MSG_TYPE_PING = 2;

	virtual void enqueueMessages(int count, const net::Message *msgs) = 0;
	virtual void enqueuePing(bool pong) = 0;

	virtual QAbstractSocket::SocketState getSocketState() = 0;
	virtual void abortSocket() = 0;

	void handlePing(bool isPong);

	bool m_decodeOpaque;
	net::MessageList m_inbox; // received (complete) messages
	bool m_gracefullyDisconnecting;
	// Smoothing of received messages. Depending on the server and network
	// conditions, messages will arrive in chunks, which causes other people's
	// strokes to appear choppy. Smoothing compensates for it, if enabled.
	bool m_smoothEnabled;
	int m_smoothDrainRate;
	QTimer *m_smoothTimer;
	net::MessageList m_smoothBuffer;
	int m_smoothMessagesToDrain;
	unsigned int m_contextId;
	qint64 m_lastRecvTime;

private slots:
	void checkIdleTimeout();
	void sendArtificallyLaggedMessages();

private:
	static constexpr int SMOOTHING_INTERVAL_MSEC = 1000 / 60;

	virtual void afterDisconnectSent() = 0;

	void sendPingMsg(bool pong);

	void updateSmoothing();

	QTimer *m_idleTimer;
	QTimer *m_pingTimer;
	qint64 m_idleTimeout;
	qint64 m_pingSent;

	int m_artificialLagMs;
	QVector<long long> m_artificialLagTimes;
	QVector<net::Message> m_artificialLagMessages;
	QTimer *m_artificialLagTimer;
};

}

#endif
