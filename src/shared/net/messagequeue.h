/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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

#ifndef DP_NET_MSGQUEUE_H
#define DP_NET_MSGQUEUE_H

#include <QQueue>
#include <QObject>

#include "message.h"

class QTcpSocket;
class QTimer;

namespace protocol {

/**
 * A wrapper for an IO device for sending and receiving messages.
 */
class MessageQueue : public QObject {
Q_OBJECT
public:
	/**
	 * @brief Create a message queue that wraps a TCP socket.
	 *
	 * The MessageQueue does not take ownership of the device.
	 */
	explicit MessageQueue(QTcpSocket *socket, QObject *parent=0);
	MessageQueue(const MessageQueue&) = delete;
	~MessageQueue();

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
	 * @brief Check if there are new snapshot messages available
	 * This command is used only on the server
	 * @return true if getPendingSnapshot will return a message
	 */
	bool isPendingSnapshot() const;

	/**
	 * @brief Get the next snapshot message in the queue.
	 *
	 * This command is used only on the server. A SnapshotMode::END indicates
	 * the end of the snapshot.
	 * @return next snapshot message
	 */
	MessagePtr getPendingSnapshot();

	/**
	 * Enqueue a message for sending.
	 */
	void send(MessagePtr message);

	/**
	 * @brief Set the snapshot to upload
	 *
	 * This method is used to enqueue a snapshot point for asynchronous upload.
	 * Messages from the snapshot queue are sent when there is a lull in the main queue.
	 * This command is used only on the client side.
	 *
	 * Note. Calling this function will overwrite the old snapshot queue, so make
	 * sure it has been fully sent first!
	 * @param snapshot
	 */
	void sendSnapshot(const QList<MessagePtr> &snapshot);

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
	void setRandomLag(uint lag) { _randomlag = lag; }
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
	 * @brief information about the amount of data to be received
	 * @param count
	 */
	void expectingBytes(int count);

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
	 * New snapshot message(s) are available. Get them with getPendingSnapshot().
	 */
	void snapshotAvailable();

	/**
	 * All queued messages have been sent
	 */
	void allSent();

	/**
	 * An unrecognized message was received
	 * @param len length of the unrecognized message
	 * @param the unknown message identifier
	 */
	void badData(int len, int type);

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

	QTcpSocket *_socket;

	char *_recvbuffer;
	char *_sendbuffer;
	int _recvcount;
	int _sentcount, _sendbuflen;

	QQueue<MessagePtr> _recvqueue;
	QQueue<MessagePtr> _sendqueue;

	QQueue<MessagePtr> _snapshot_recv;
	QList<MessagePtr> _snapshot_send;

	QTimer *_idleTimer;
	QTimer *_pingTimer;
	qint64 _lastRecvTime;
	qint64 _idleTimeout;
	qint64 _pingSent;

	bool _closeWhenReady;
	bool _expectingSnapshot;
	bool _ignoreIncoming;

#ifndef NDEBUG
	uint _randomlag;
#endif
};

}

#endif

