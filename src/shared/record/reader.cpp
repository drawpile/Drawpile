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
#include <QFile>

#include <cstring>

#include "reader.h"
#include "util.h"
#include "../net/recording.h"

#include "config.h"

namespace recording {

Reader::Reader(const QString &filename, QObject *parent)
	: Reader(new QFile(filename), true, parent)
{
}

Reader::Reader(QFileDevice *file, bool autoclose, QObject *parent)
	: QObject(parent), _file(file), _autoclose(autoclose), _current(-1)
{
	Q_ASSERT(file);
}

Reader::~Reader()
{
	if(_autoclose)
		delete _file;
}

Compatibility Reader::open()
{
	if(!_file->isOpen()) {
		if(!_file->open(QFile::ReadOnly))
			return CANNOT_READ;
	}

	// Read magic bytes "DPRECR\0"
	char buf[7];
	if(_file->read(buf, 7) != 7)
		return CANNOT_READ;

	if(memcmp(buf, "DPRECR", 7))
		return NOT_DPREC;

	// Read protocol version
	if(_file->read(buf, 4) != 4)
		return NOT_DPREC;
	quint32 protover = qFromBigEndian<quint32>((const uchar*)buf);

	// Read program version
	QByteArray progver;
	do {
		if(_file->read(buf, 1) != 1)
			return NOT_DPREC;
		progver.append(buf[0]);
	} while(buf[0] != '\0');
	_writerversion = QString::fromUtf8(progver);

	_beginning = _file->pos();

	// Decide if we are compatible
	quint32 myversion = version32(DRAWPILE_PROTO_MAJOR_VERSION, DRAWPILE_PROTO_MINOR_VERSION);

	// Best case
	if(myversion == protover)
		return COMPATIBLE;

	// Recording made with a newer version. It may include unsupported commands.
	if(myversion < protover) {
		// If major version is same, expect only minor incompatabilities
		if(majorVersion(myversion) == majorVersion(protover))
			return MINOR_INCOMPATIBILITY;

		return UNKNOWN_COMPATIBILITY;
	}

	// Recording made with an older version.
	// Version 7.1 is fully compatible with current
	if(protover >= version32(7, 1))
		return COMPATIBLE;

	// Older release with same major version: we know there are minor incompatabilities
	if(majorVersion(myversion) == majorVersion(protover))
		return MINOR_INCOMPATIBILITY;

	// Older versions are incompatible
	return INCOMPATIBLE;
}

QString Reader::errorString() const
{
	return _file->errorString();
}

QString Reader::filename() const
{
	return _file->fileName();
}

qint64 Reader::filesize() const
{
	return _file->size();
}

qint64 Reader::filePosition() const
{
	return _file->pos();
}

void Reader::close()
{
	Q_ASSERT(_file->isOpen());
	_file->close();
}

void Reader::rewind()
{
	_file->seek(_beginning);
	_current = -1;
	_currentPos = -1;
}

void Reader::seekTo(int pos, qint64 position)
{
	_current = pos;
	_currentPos = position;
	_file->seek(position);
}

bool Reader::readNextToBuffer(QByteArray &buffer)
{
	// Read length and type header
	if(buffer.length() < 3)
		buffer.resize(1024);

	_currentPos = filePosition();

	if(_file->read(buffer.data(), 3) != 3)
		return false;

	const int len = protocol::Message::sniffLength(buffer.constData());
	Q_ASSERT(len>=3); // fixed header length should be included

	if(buffer.length() < len)
		buffer.resize(len);

	// Read message payload
	const int payloadlen = len - 3;
	if(_file->read(buffer.data()+3, payloadlen) != payloadlen)
		return false;

	++_current;

	return true;
}

MessageRecord Reader::readNext()
{
	MessageRecord msg;

	if(!readNextToBuffer(_msgbuf))
		return msg;

	auto *message = protocol::Message::deserialize((const uchar*)_msgbuf.constData());

	if(message) {
		msg.status = MessageRecord::OK;
		msg.message = message;
	} else {
		msg.status = MessageRecord::INVALID;
		msg.len = protocol::Message::sniffLength(_msgbuf.constData());
		msg.type = protocol::MessageType(_msgbuf.at(2));
	}

	return msg;
}

}
