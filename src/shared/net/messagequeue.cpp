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
#include <QTcpSocket>
#include <cstring>

#include "messagequeue.h"
#include "snapshot.h"
#include "flow.h"

namespace protocol {

// Reserve enough buffer space for one complete message + snapshot mode marker
static const int MAX_BUF_LEN = 1024*64 + 4 + 5;

MessageQueue::MessageQueue(QTcpSocket *socket, QObject *parent)
	: QObject(parent), _socket(socket), _closeWhenReady(false), _expectingSnapshot(false),
	  _ignoreIncoming(false)
{
	connect(socket, SIGNAL(readyRead()), this, SLOT(readData()));
	connect(socket, SIGNAL(bytesWritten(qint64)), this, SLOT(dataWritten(qint64)));

	if(socket->inherits("QSslSocket")) {
		connect(socket, SIGNAL(encrypted()), this, SLOT(sslEncrypted()));
	}

	_recvbuffer = new char[MAX_BUF_LEN];
	_sendbuffer = new char[MAX_BUF_LEN];
	_recvcount = 0;
	_sentcount = 0;
	_sendbuflen = 0;
}

void MessageQueue::sslEncrypted()
{
	disconnect(_socket, SIGNAL(bytesWritten(qint64)), this, SLOT(dataWritten(qint64)));
	connect(_socket, SIGNAL(encryptedBytesWritten(qint64)), this, SLOT(dataWritten(qint64)));
}

MessageQueue::~MessageQueue()
{
	delete [] _recvbuffer;
	delete [] _sendbuffer;
}

bool MessageQueue::isPending() const
{
	return !_recvqueue.isEmpty();
}

MessagePtr MessageQueue::getPending()
{
	return _recvqueue.dequeue();
}

bool MessageQueue::isPendingSnapshot() const
{
	return !_snapshot_recv.isEmpty();
}

MessagePtr MessageQueue::getPendingSnapshot()
{
	return _snapshot_recv.dequeue();
}

void MessageQueue::send(MessagePtr packet)
{
	if(!_closeWhenReady) {
		_sendqueue.enqueue(packet);
		if(_sendbuflen==0)
			writeData();
	}
}

void MessageQueue::sendSnapshot(const QList<MessagePtr> &snapshot)
{
	if(!_closeWhenReady) {
		_snapshot_send = snapshot;
		_snapshot_send.append(MessagePtr(new SnapshotMode(SnapshotMode::END)));

		if(_sendbuflen==0)
			writeData();
	}
}

void MessageQueue::sendDisconnect(int reason, const QString &message)
{
	send(MessagePtr(new protocol::Disconnect(protocol::Disconnect::Reason(reason), message)));
	_ignoreIncoming = true;
	_recvcount = 0;
}

int MessageQueue::uploadQueueBytes() const
{
	int total = _socket->bytesToWrite() + _sendbuflen - _sentcount;
	foreach(const MessagePtr msg, _sendqueue)
		total += msg->length();
	foreach(const MessagePtr msg, _snapshot_send)
		total += msg->length() + 4; /* include snapshot mode packets */
	return total;
}

void MessageQueue::readData() {
	bool gotmessage = false, gotsnapshot = false;
	int read, totalread=0;
	do {
		// Read available data
		read = _socket->read(_recvbuffer+_recvcount, MAX_BUF_LEN-_recvcount);
		if(read<0) {
			// Error!
			emit socketError(_socket->errorString());
			return;
		}

		if(_ignoreIncoming) {
			// Ignore incoming data mode is used when we're shutting down the connection
			// but want to clear the upload queue
			if(read>0)
				continue;
			else
				return;
		}

		_recvcount += read;

		// Extract all complete messages
		int len;
		while(_recvcount >= Message::HEADER_LEN && _recvcount >= (len=Message::sniffLength(_recvbuffer))) {
			// Whole message received!
			Message *msg = Message::deserialize((const uchar*)_recvbuffer, _recvcount);
			if(!msg) {
				emit badData(len, _recvbuffer[2]);
			} else {
				if(msg->type() == MSG_STREAMPOS) {
					// Special handling for Stream Position message
					emit expectingBytes(static_cast<StreamPos*>(msg)->bytes() + totalread);
					delete msg;
				} else if(_expectingSnapshot) {
					// A message preceded by SnapshotMode::SNAPSHOT goes into the snapshot queue
					_snapshot_recv.enqueue(MessagePtr(msg));
					_expectingSnapshot = false;
					gotsnapshot = true;
				} else {
					if(msg->type() == MSG_SNAPSHOT && static_cast<SnapshotMode*>(msg)->mode() == SnapshotMode::SNAPSHOT) {
						delete msg;
						_expectingSnapshot = true;
					} else {
						_recvqueue.enqueue(MessagePtr(msg));
						gotmessage = true;
					}
				}
			}
			if(len < _recvcount) {
				memmove(_recvbuffer, _recvbuffer+len, _recvcount-len);
			}
			_recvcount -= len;
		}
		totalread += read;
	} while(read>0);

	if(totalread)
		emit bytesReceived(totalread);
	if(gotmessage)
		emit messageAvailable();
	if(gotsnapshot)
		emit snapshotAvailable();
}

void MessageQueue::dataWritten(qint64 bytes)
{
	emit bytesSent(bytes);

	// Write more once the buffer is empty
	if(_socket->bytesToWrite()==0) {
		if(_sendbuflen==0 && _sendqueue.isEmpty() && _snapshot_send.isEmpty())
			emit allSent();
		else
			writeData();
	}
}

void MessageQueue::writeData() {
	if(_sendbuflen==0) {
		// If send buffer is empty, serialize the next message in the queue.
		// The snapshot upload queue has lower priority than the normal queue.
		if(!_sendqueue.isEmpty()) {
			// There are messages in the higher priority queue, send one
			MessagePtr msg = _sendqueue.dequeue();
			_sendbuflen = msg->serialize(_sendbuffer);
			if(msg->type() == protocol::MSG_DISCONNECT) {
				// Automatically disconnect after Disconnect notification is sent
				_closeWhenReady = true;
				_sendqueue.clear();
			}

		} else if(!_snapshot_send.isEmpty()) {
			// When the main send queue is empty, messages from the snapshot queue are sent
			SnapshotMode mode(SnapshotMode::SNAPSHOT);
			_sendbuflen = mode.serialize(_sendbuffer);
			_sendbuflen += _snapshot_send.takeFirst()->serialize(_sendbuffer + _sendbuflen);
		}
	}

	if(_sentcount < _sendbuflen) {
		int sent = _socket->write(_sendbuffer+_sentcount, _sendbuflen-_sentcount);
		if(sent<0) {
			// Error
			emit socketError(_socket->errorString());
			return;
		}
		_sentcount += sent;
		if(_sentcount == _sendbuflen) {
			_sendbuflen=0;
			_sentcount=0;
			if(_closeWhenReady) {
				_socket->disconnectFromHost();

			} else {
				writeData();
			}
		}
	}
}

}

