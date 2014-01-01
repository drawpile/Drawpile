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

#include <cstring>

#include "reader.h"
#include "util.h"
#include "../net/recording.h"

#include "config.h"

namespace recording {

Reader::Reader(const QString &filename, QObject *parent)
	: QObject(parent), _file(filename)
{
}

Reader::~Reader()
{

}

Compatibility Reader::open()
{
	if(!_file.open(QFile::ReadOnly))
		return CANNOT_READ;

	// Read first 5 bytes: expect magic bytes "DPREC"
	char buf[5];
	if(_file.read(buf, 5) != 5)
		return CANNOT_READ;

	if(memcmp(buf, "DPREC", 5))
		return NOT_DPREC;

	// Next byte is either 'R' for raw recording or 'I' for indexed.
	char rectype;
	if(_file.read(&rectype, 1) != 1)
		return NOT_DPREC;

	// Expect 0 byte
	if(_file.read(buf, 1) != 1)
		return NOT_DPREC;

	if(buf[0] != 0)
		return NOT_DPREC;

	// Read protocol version
	if(_file.read(buf, 4) != 4)
		return NOT_DPREC;
	quint32 protover = qFromBigEndian<quint32>((const uchar*)buf);

	// Read program version
	QByteArray progver;
	do {
		if(_file.read(buf, 1) != 1)
			return NOT_DPREC;
		progver.append(buf[0]);
	} while(buf[0] != '\0');
	_writerversion = QString::fromUtf8(progver);

	// Decide if we are compatible

	// only raw recordings are currently supported
	if(rectype != 'R')
		return INCOMPATIBLE;

	quint32 myversion = version32(DRAWPILE_PROTO_MAJOR_VERSION, DRAWPILE_PROTO_MINOR_VERSION);

	// Best case
	if(myversion == protover)
		return COMPATIBLE;

	// Same major versions: expect only minor incompatibility
	if(majorVersion(myversion) == majorVersion(protover))
		return MINOR_INCOMPATIBILITY;

	// Recording made with a newer version. It may include unsupported commands.
	if(myversion < protover)
		return UNKNOWN_COMPATIBILITY;

	// Recording made with an older version.
	// TODO: there are no older versions at the moment
	return UNKNOWN_COMPATIBILITY;
}

QString Reader::errorString() const
{
	return _file.errorString();
}

void Reader::close()
{
	Q_ASSERT(_file.isOpen());
	_file.close();
}

MessageRecord Reader::readNext()
{
	MessageRecord msg;

	// Read length and type header
	char header[3];
	if(_file.read(header, 2) != 2)
		return msg;
	int len = qFromBigEndian<quint16>((uchar*)header);

	if(_file.read(header+2, 1) != 1)
		return msg;
	protocol::MessageType msgtype = protocol::MessageType(header[2]);

	// Read message payload
	QVarLengthArray<char> buf(len+3);
	memcpy(buf.data(), header, 3);

	if(_file.read(buf.data()+3, len) != len)
		return msg;

	// Deserialize message
	protocol::Message *message = protocol::Message::deserialize((const uchar*)buf.data());

	if(message) {
		msg.status = MessageRecord::OK;
		msg.message = message;
	} else {
		msg.status = MessageRecord::INVALID;
		msg.len = len;
		msg.type = msgtype;
	}

	return msg;
}

}
