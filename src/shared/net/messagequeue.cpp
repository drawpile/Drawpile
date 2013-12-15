/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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

#include <QDebug>
#include <QIODevice>
#include <cstring>

#include "messagequeue.h"
#include "snapshot.h"
#include "meta.h" /* for STREAMPOS */

namespace protocol {

// Reserve enough buffer space for one complete message + snapshot mode marker
static const int MAX_BUF_LEN = 1024*64 + 4 + 5;

MessageQueue::MessageQueue(QIODevice *socket, QObject *parent)
	: QObject(parent), _socket(socket), _closeWhenReady(false), _expectingSnapshot(false)
{
	connect(socket, SIGNAL(readyRead()), this, SLOT(readData()));
	connect(socket, SIGNAL(bytesWritten(qint64)), this, SLOT(writeData()));

	_recvbuffer = new char[MAX_BUF_LEN];
	_sendbuffer = new char[MAX_BUF_LEN];
	_recvcount = 0;
	_sentcount = 0;
	_sendbuflen = 0;
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
		if(_sentcount==0)
			writeData();
	}
}

void MessageQueue::sendSnapshot(const QList<MessagePtr> &snapshot)
{
	if(!_closeWhenReady) {
		_snapshot_send = snapshot;
		_snapshot_send.append(MessagePtr(new SnapshotMode(SnapshotMode::END)));

		if(_sentcount==0)
			writeData();
	}
}

int MessageQueue::uploadQueueBytes() const
{
	int total = _sendbuflen - _sentcount;
	foreach(const MessagePtr msg, _sendqueue)
		total += msg->length();
	foreach(const MessagePtr msg, _snapshot_send)
		total += msg->length();
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
		_recvcount += read;

		// Extract all complete messages
		int len;
		while(2 < _recvcount && (len=Message::sniffLength(_recvbuffer)) <= _recvcount) {
			// Whole message received!
			Message *msg = Message::deserialize((const uchar*)_recvbuffer);
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

void MessageQueue::writeData() {
	if(_sendbuflen==0) {
		// If send buffer is empty, serialize the next message in the queue

		if(_sendqueue.isEmpty() && _snapshot_send.isEmpty()) {
			emit allSent();
			return;
		}

		if(_sendqueue.isEmpty()) {
			// If there is nothing in the normal send queue, there should
			// there should be something in the lower priority snapshot queue
			SnapshotMode mode(SnapshotMode::SNAPSHOT);
			_sendbuflen = mode.serialize(_sendbuffer);
			_sendbuflen += _snapshot_send.takeFirst()->serialize(_sendbuffer + _sendbuflen);
		} else {
			// There are messages in the higher priority queue, send one
			_sendbuflen = _sendqueue.dequeue()->serialize(_sendbuffer);
		}
	}

	if(_sentcount < _sendbuflen) {
		int sent = _socket->write(_sendbuffer+_sentcount, _sendbuflen-_sentcount);
		if(sent<0) {
			// Error
			emit socketError(_socket->errorString());
			return;
		}
		emit bytesSent(sent);
		_sentcount += sent;
		if(_sentcount == _sendbuflen) {
			_sendbuflen=0;
			_sentcount=0;
			if(_closeWhenReady)
				close();
			else
				writeData();
		}
	}
}

void MessageQueue::close() {
	_socket->close();
	_closeWhenReady = false;
}

/**
 * The socket is closed as soon as all pending data has been written.
 * No further data is accepted for transmission after closeWhenReady()
 * has been called.
 */
void MessageQueue::closeWhenReady() {
	if(_sendbuflen==0)
		close();
	else
		_closeWhenReady = true;
}

#if 0
void MessageQueue::flush() {
	_sendbuffer.clear();
	_recvbuffer.clear();
	_recvqueue.clear();
	_expecting = 0;
}
#endif
}

