/*
   DrawPile - a collaborative drawing program.

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

class QIODevice;

namespace protocol {

/**
 * A wrapper for an IO device for sending and receiving messages.
 */
class MessageQueue : public QObject {
Q_OBJECT
public:
	/**
	 * Create a message queue that wraps an IO device (typicaly a tcp socket).
	 * The messagequeue does not take ownership of the device.
	 */
	MessageQueue(QIODevice *socket, QObject *parent=0);
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
	 * Close the IO device
	 */
	void close();

	/**
	 * Close the IO device as soon as the current message has been sent.
	 * written.
	 */
	void closeWhenReady();

	/**
	 * @brief Get the number of bytes in the upload queue
	 * @return
	 */
	int uploadQueueBytes() const;

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

private slots:
	void readData();
	void dataWritten(qint64);

private:
	void writeData();

	QIODevice *_socket;

	char *_recvbuffer;
	char *_sendbuffer;
	int _recvcount;
	int _sentcount, _sendbuflen;

	QQueue<MessagePtr> _recvqueue;
	QQueue<MessagePtr> _sendqueue;

	QQueue<MessagePtr> _snapshot_recv;
	QList<MessagePtr> _snapshot_send;

	bool _closeWhenReady;
	bool _expectingSnapshot;
};

}

#endif

