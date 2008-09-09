#include <QIODevice>

#include "protocol.h"
#include "messagequeue.h"

namespace protocol {

MessageQueue::MessageQueue(QIODevice *socket, QObject *parent)
	: QObject(parent), _socket(socket), _expecting(0)
{
	connect(socket, SIGNAL(readyRead()), this, SLOT(readData()));
	connect(socket, SIGNAL(bytesWritten(qint64)), this, SLOT(writeData(qint64)));
}

Packet *MessageQueue::getPending() {
	if(_recvqueue.isEmpty())
		return 0;
	return _recvqueue.dequeue();
}

void MessageQueue::send(const Packet& packet) {
	send(packet.serialize());
}

void MessageQueue::send(const QByteArray& data) {
	bool wasEmpty = _sendbuffer.isEmpty();
	_sendbuffer.append(data);
	if(wasEmpty)
		writeData(0);
}

void MessageQueue::readData() {
	// Append data to our internal buffer
	_recvbuffer.append(_socket->readAll());

	// We have enough to at least know how many bytes we need. Loop
	// until we have extracted all the packets we can.
	bool signal = false;
	while(_recvbuffer.length()>=3) {
		if(_expecting==0) {
			_expecting = Packet::sniffLength(_recvbuffer);
			if(_expecting==0) {
				emit badData();
				return;
			}
		}
		if(_expecting <= _recvbuffer.length()) {
			Packet *pkt = Packet::deserialize(_recvbuffer);
			if(pkt==0) {
				emit badData();
				return;
			}
			signal = true;
			_recvbuffer = _recvbuffer.mid(pkt->length());
			_recvqueue.enqueue(pkt);
			_expecting=0;
		} else {
			break;
		}
	}
	if(signal)
		emit messageAvailable();
}

void MessageQueue::writeData(qint64 prev) {
	if(_sendbuffer.isEmpty()==false) {
		int written = _socket->write(_sendbuffer);
		if(written!=-1)
			_sendbuffer = _sendbuffer.mid(written);
	} else if(_closeWhenReady)
		_socket->close();
}

void MessageQueue::close() {
	_socket->close();
}

void MessageQueue::closeWhenReady() {
	if(_sendbuffer.isEmpty())
		_socket->close();
	else
		_closeWhenReady = true;
}

void MessageQueue::flush() {
	_sendbuffer.clear();
	_recvbuffer.clear();
	_recvqueue.clear();
	_expecting = 0;
}

}

