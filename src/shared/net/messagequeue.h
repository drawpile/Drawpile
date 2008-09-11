/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#ifndef DP_NET_MSGQUEUE_H
#define DP_NET_MSGQUEUE_H

#include <QQueue>
#include <QObject>

class QIODevice;

namespace protocol {

class Packet;

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

		/**
		 * Get the next message in the queue.
		 * @return message or null if no message was waiting
		 */
		Packet *getPending();

		/**
		 * Enqueue a message for sending.
		 */
		void send(const Packet& packet);

		/**
		 * Enqueue raw data for sending.
		 */
		void send(const QByteArray& data);

		/**
		 * Close the IO device
		 */
		void close();

		/**
		 * Close the IO device as soon as all pending data has been
		 * written.
		 */
		void closeWhenReady();

		/**
		 * Clear all queues and buffers.
		 */
		void flush();

	signals:
		/**
		 * A new message is available. Get it with getPending(). Note.
		 * Multiple messages may be enqueued per single signal emission.
		 */
		void messageAvailable();

		/**
		 * Invalid data was received
		 */
		void badData();

	private slots:
		void readData();
		void writeData(qint64 prev);

	private:
		QIODevice *_socket;
		QByteArray _recvbuffer, _sendbuffer;
		QQueue<Packet*> _recvqueue;
		int _expecting;
		bool _closeWhenReady;
};

}

#endif

