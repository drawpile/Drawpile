/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include "reader.h"
#include "reader-compat.h"
#include "util.h"
#include "../net/recording.h"

#include "config.h"

#include <QtEndian>
#include <QVarLengthArray>
#include <QFile>
#include <KCompressionDevice>
#include <QRegularExpression>

#include <cstring>

namespace recording {

bool Reader::isRecordingExtension(const QString &filename)
{
	QRegularExpression re("\\.dprec(?:z|\\.(?:gz|bz2|xz))?$");
	return re.match(filename).hasMatch();
}

Reader::Reader(const QString &filename, QObject *parent)
	: QObject(parent), _filename(filename), _current(-1), _autoclose(true), _eof(false), _isHibernation(false), _compat(true)
{
	KCompressionDevice::CompressionType ct = KCompressionDevice::None;
	if(filename.endsWith(".gz", Qt::CaseInsensitive) || filename.endsWith(".dprecz", Qt::CaseInsensitive))
		ct = KCompressionDevice::GZip;
	else if(filename.endsWith(".bz2", Qt::CaseInsensitive))
		ct = KCompressionDevice::BZip2;
	else if(filename.endsWith(".xz", Qt::CaseInsensitive))
		ct = KCompressionDevice::Xz;

	if(ct == KCompressionDevice::None) {
		_file = new QFile(filename);
		_isCompressed = false;
	} else {
		_file = new KCompressionDevice(filename, ct);
		_isCompressed = true;
	}
}

Reader::Reader(const QString &filename, QIODevice *file, bool autoclose, QObject *parent)
	: QObject(parent), _filename(filename), _file(file), _current(-1), _autoclose(autoclose), _eof(false), _isHibernation(false), _isCompressed(false), _compat(true)
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
		if(!_file->open(QFile::ReadOnly)) {
			return CANNOT_READ;
		}
	}

	// Read magic bytes "DPRECR\0" or "DPRECH\0"
	char buf[7];
	if(_file->read(buf, 7) != 7)
		return CANNOT_READ;

	if(memcmp(buf, "DPRECR", 7)) {
		if(memcmp(buf, "DPRECH", 7))
			return NOT_DPREC;
		_isHibernation = true;
	}

	// Read protocol version
	if(_file->read(buf, 4) != 4)
		return NOT_DPREC;
	_formatversion = qFromBigEndian<quint32>((const uchar*)buf);

	// Read program version
	QByteArray progver;
	do {
		if(_file->read(buf, 1) != 1)
			return NOT_DPREC;
		progver.append(buf[0]);
	} while(buf[0] != '\0');
	_writerversion = QString::fromUtf8(progver);

	// If this is a hibernation file, read the rest of the header
	if(_isHibernation) {
		// We already read the protocol minor version
		_hibheader.minorVersion = minorVersion(_formatversion);

		// Check hibernation file format version
		char fmtver;
		if(!_file->getChar(&fmtver))
			return NOT_DPREC;

		// Currently there is only one format and no forward compatibility
		if((uchar)fmtver != 1)
			return INCOMPATIBLE;

		// Read session title
		if(_file->read(buf, 2) != 2)
			return NOT_DPREC;

		quint16 titleLen = qFromBigEndian<quint16>((const uchar*)buf);
		QByteArray title = _file->read(titleLen);
		if(title.length() != titleLen)
			return NOT_DPREC;

		_hibheader.title = QString::fromUtf8(title);

		// Read session founder name
		if(_file->read(buf, 1) != 1)
				return NOT_DPREC;
		quint8 founderLen = *buf;

		QByteArray founder = _file->read(founderLen);
		if(founder.length() != founderLen)
			return NOT_DPREC;

		_hibheader.founder = QString::fromUtf8(founder);

		// Read session flags
		char flags;
		if(!_file->getChar(&flags))
			return NOT_DPREC;
		_hibheader.flags = HibernationHeader::Flags((uchar)flags);

		// Read session password
		if(_file->read(buf, 2) != 2)
			return NOT_DPREC;

		quint16 passwdLen = qFromBigEndian<quint16>((const uchar*)buf);
		_hibheader.password = _file->read(passwdLen);
		if(_hibheader.password.length() != passwdLen)
			return NOT_DPREC;
	}

	_beginning = _file->pos();

	const quint32 myversion = version32(DRAWPILE_PROTO_MAJOR_VERSION, DRAWPILE_PROTO_MINOR_VERSION);

	// Check version number. Best case is exact match.
	if(myversion == _formatversion)
		return COMPATIBLE;

	if(_compat) {
		// Compatability mode: support other versions

		// A recording made with a newer (major) version may contain unsupported commands.
		if(majorVersion(myversion) < majorVersion(_formatversion))
			return UNKNOWN_COMPATIBILITY;

		// Newer minor version: expect rendering differences
		if(myversion < _formatversion)
			return MINOR_INCOMPATIBILITY;

#if DRAWPILE_PROTO_MAJOR_VERSION != 15 || DRAWPILE_PROTO_MINOR_VERSION != 5
#error Update recording compatability check!
#endif

		// Old versions known to be compatible
		switch(_formatversion) {
		case version32(14, 5): // fully compatible (with support code)
		case version32(13, 5):
		case version32(13, 4):
		case version32(12, 4):
			return COMPATIBLE;

		case version32(11, 3): // supported, but expect minor rendering differences
		case version32(11, 2):
		case version32(10, 2):
		case version32(9, 2):
		case version32(8, 1):
		case version32(7, 1):
			return MINOR_INCOMPATIBILITY;
		}

		// Other versions are not supported
		return INCOMPATIBLE;

	} else {
		// Strict version mode: major version number must match

		// (we already checked the exact match above)
		if(majorVersion(myversion) == majorVersion(_formatversion))
			return MINOR_INCOMPATIBILITY;

		return INCOMPATIBLE;
	}
}

QString Reader::errorString() const
{
	return _file->errorString();
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
	_eof = false;
}

void Reader::seekTo(int pos, qint64 position)
{
	_current = pos;
	_currentPos = position;
	_file->seek(position);
	_eof =false;
}

bool Reader::readNextToBuffer(QByteArray &buffer)
{
	// Read length and type header
	if(buffer.length() < protocol::Message::HEADER_LEN)
		buffer.resize(1024);

	_currentPos = filePosition();

	if(_file->read(buffer.data(), protocol::Message::HEADER_LEN) != protocol::Message::HEADER_LEN) {
		_eof = true;
		return false;
	}

	const int len = protocol::Message::sniffLength(buffer.constData());

	if(buffer.length() < len)
		buffer.resize(len);

	// Read message payload
	const int payloadlen = len - protocol::Message::HEADER_LEN;
	if(_file->read(buffer.data()+protocol::Message::HEADER_LEN, payloadlen) != payloadlen) {
		_eof = true;
		return false;
	}

	++_current;

	return true;
}

MessageRecord Reader::readNext()
{
	MessageRecord msg;

	if(!readNextToBuffer(_msgbuf))
		return msg;

	protocol::Message *message;
	if(_compat && _formatversion != version32(DRAWPILE_PROTO_MAJOR_VERSION, DRAWPILE_PROTO_MINOR_VERSION)) {

		// see protocol changelog in doc/protocol.md
		switch(_formatversion) {
		case version32(14, 5):
		case version32(13, 5):
			message = compat::deserializeV14((const uchar*)_msgbuf.constData(), _msgbuf.length());
			break;

		case version32(12, 4):
			message = compat::deserializeV12((const uchar*)_msgbuf.constData(), _msgbuf.length());
			break;

		case version32(11, 3):
		case version32(11, 2):
			message = compat::deserializeV11((const uchar*)_msgbuf.constData(), _msgbuf.length());
			break;

		case version32(10, 2):
		case version32(9, 2):
		case version32(8, 1):
		case version32(7, 1):
			message = compat::deserializeV10((const uchar*)_msgbuf.constData(), _msgbuf.length());
			break;

		default:
			message = protocol::Message::deserialize((const uchar*)_msgbuf.constData(), _msgbuf.length());
			break;
		}
	} else {
		message = protocol::Message::deserialize((const uchar*)_msgbuf.constData(), _msgbuf.length());
	}

	if(message) {
		msg.status = MessageRecord::OK;
		msg.message = message;
	} else {
		msg.status = MessageRecord::INVALID;
		msg.error.len = protocol::Message::sniffLength(_msgbuf.constData());
		msg.error.type = protocol::MessageType(_msgbuf.at(2));
	}

	return msg;
}

}
