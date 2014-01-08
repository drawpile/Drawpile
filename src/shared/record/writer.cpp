/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#include <QtEndian>
#include <QVarLengthArray>
#include <QDateTime>

#include "writer.h"
#include "util.h"
#include "../net/recording.h"

#include "config.h"

namespace recording {

Writer::Writer(const QString &filename, QObject *parent)
	: QObject(parent), _file(filename), _writeIntervals(false)
{
}

Writer::~Writer()
{
}

void Writer::setWriteIntervals(bool wi)
{
	_writeIntervals = wi;
	_interval = QDateTime::currentMSecsSinceEpoch();
}

bool Writer::open()
{
	return _file.open(QFile::WriteOnly);
}

QString Writer::errorString() const
{
	return _file.errorString();
}

bool Writer::writeHeader()
{
	Q_ASSERT(_file.isOpen());

	// Format identification
	const char *MAGIC = "DPRECR";
	_file.write(MAGIC, 7);

	// Protocol version
	uchar protover[4];
	qToBigEndian(version32(DRAWPILE_PROTO_MAJOR_VERSION, DRAWPILE_PROTO_MINOR_VERSION), protover);
	_file.write((const char*)protover, 4);

	// Program version
	const char *VERSION = DRAWPILE_VERSION;
	_file.write(VERSION, qstrlen(VERSION)+1);

	return true;
}

namespace {
bool isRecordableMeta(protocol::MessageType type) {
	switch(type) {
	using namespace protocol;
	case MSG_USER_JOIN:
	case MSG_USER_LEAVE:
	case MSG_CHAT:
	case MSG_SESSION_TITLE:
	case MSG_INTERVAL:
		return true;
	default:
		return false;
	}
}
}

void Writer::recordMessage(const protocol::MessagePtr msg)
{
	Q_ASSERT(_file.isOpen());

	if(msg->isCommand() || isRecordableMeta(msg->type())) {
		// Write Interval message if sufficient time has passed since the last one
		if(_writeIntervals) {
			qint64 now = QDateTime::currentMSecsSinceEpoch();
			qint64 interval = now - _interval;
			if(interval >= (1000/20)) {
				protocol::Interval imsg(qMin(qint64(0xffff), interval));
				QVarLengthArray<char> ibuf(imsg.length());
				int ilen = imsg.serialize(ibuf.data());
				_file.write(ibuf.data(), ilen);
				_interval = now;
			}
		}

		// Write the actual message
		QVarLengthArray<char> buf(msg->length());
		int len = msg->serialize(buf.data());
		Q_ASSERT(len == buf.length());
		_file.write(buf.data(), len);
	}
}

void Writer::close()
{
	Q_ASSERT(_file.isOpen());
	_file.close();
}

}
