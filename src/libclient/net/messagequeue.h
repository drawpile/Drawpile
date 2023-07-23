// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_CLIENT_MSGQUEUE_H
#define DP_CLIENT_MSGQUEUE_H

#include "libclient/drawdance/message.h"

#include <QQueue>
#include <QVector>
#include <QObject>

class QTcpSocket;
class QTimer;

namespace net {

/**
 * A wrapper for an IO device for sending and receiving messages.
 */
class MessageQueue final : public QObject {
Q_OBJECT
public:
	enum class GracefulDisconnect {
		Error, // An error occurred
		Kick, // client was kicked by the session operator
		Shutdown, // server is shutting down
		Other, // other unspecified error
	};

	/**
	 * @brief Create a message queue that wraps a TCP socket.
	 *
	 * The MessageQueue does not take ownership of the device.
	 */
	explicit MessageQueue(QTcpSocket *socket, QObject *parent=nullptr);
	~MessageQueue() override;

	/**
	 * @brief Check if there are new messages available
	 * @return true if getPending will return a message
	 */
	bool isPending() const;

	/**
	 * Get received messages, swaps (!) the given buffer with the inbox.
	 */
	void receive(drawdance::MessageList &buffer);

	/**
	 * Enqueue a single message for sending.
	 */
	void send(const drawdance::Message &msg);

	/**
	 * Enqueue multiple messages for sending.
	 */
	void sendMultiple(int count, const drawdance::Message *msgs);

	/**
	 * @brief Gracefully disconnect
	 *
	 * This function enqueues the disconnect notification message. The connection will
	 * be automatically closed after the message has been sent. Additionally, it
	 * causes all incoming messages to be ignored and no more data to be accepted
	 * for sending.
	 *
	 * @param reason
	 * @param message
	 */
	void sendDisconnect(GracefulDisconnect reason, const QString &message);

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

	void setSmoothEnabled(bool smoothingEnabled);
	void setSmoothDrainRate(int smoothDrainRate);

	int artificalLagMs() { return m_artificialLagMs; }

	void setArtificialLagMs(int msecs);

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

private slots:
	void readData();
	void dataWritten(qint64);
	void sslEncrypted();
	void checkIdleTimeout();

	void receiveSmoothedMessages();

	void sendArtificallyLaggedMessages();

private:
	static constexpr int SMOOTHING_INTERVAL_MSEC = 1000 / 60;

	void enqueueMessages(int count, const drawdance::Message *msgs);

	int haveWholeMessageToRead();
	void writeData();
	bool messagesInOutbox() const;
	drawdance::Message dequeueFromOutbox();

	void handlePing(bool isPong);
	void sendPingMsg(bool pong);

	void updateSmoothing();

	QTcpSocket *m_socket;

	char *m_recvbuffer; // raw message reception buffer
	QByteArray m_sendbuffer; // raw message upload buffer
	int m_recvbytes;    // number of bytes in reception buffer
	int m_sentbytes;    // number of bytes in upload buffer already sent

	drawdance::MessageList m_inbox;  // received (complete) messages
	QQueue<drawdance::Message> m_outbox; // messages to be sent
	QQueue<bool> m_pings; // pings and pongs to be sent

	// Smoothing of received messages. Depending on the server and network
	// conditions, messages will arrive in chunks, which causes other people's
	// strokes to appear choppy. Smoothing compensates for it, if enabled.
	bool m_smoothEnabled;
	int m_smoothDrainRate;
	QTimer *m_smoothTimer;
	drawdance::MessageList m_smoothBuffer;
	int m_smoothMessagesToDrain;

	QTimer *m_idleTimer;
	QTimer *m_pingTimer;
	qint64 m_lastRecvTime;
	qint64 m_idleTimeout;
	qint64 m_pingSent;

	bool m_gracefullyDisconnecting;

	int m_artificialLagMs;
	QVector<long long> m_artificialLagTimes;
	QVector<drawdance::Message> m_artificialLagMessages;
	QTimer *m_artificialLagTimer;
};

}

#endif

