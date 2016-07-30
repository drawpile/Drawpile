/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2015 Calle Laakkonen

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
#include "util.h"
#include "../net/recording.h"

#include "config.h"

#include <QtEndian>
#include <QVarLengthArray>
#include <QFile>
#include <KCompressionDevice>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QDebug>

#include <cstring>

namespace recording {

bool Reader::isRecordingExtension(const QString &filename)
{
	QRegularExpression re("\\.dprec(?:z|\\.(?:gz|bz2|xz))?$");
	return re.match(filename).hasMatch();
}

Reader::Reader(const QString &filename, QObject *parent)
	: QObject(parent), m_filename(filename), m_current(-1), m_currentPos(0), m_autoclose(true), m_eof(false), m_isHibernation(false)
{
	KCompressionDevice::CompressionType ct = KCompressionDevice::None;
	if(filename.endsWith(".gz", Qt::CaseInsensitive) || filename.endsWith(".dprecz", Qt::CaseInsensitive))
		ct = KCompressionDevice::GZip;
	else if(filename.endsWith(".bz2", Qt::CaseInsensitive))
		ct = KCompressionDevice::BZip2;
	else if(filename.endsWith(".xz", Qt::CaseInsensitive))
		ct = KCompressionDevice::Xz;

	if(ct == KCompressionDevice::None) {
		m_file = new QFile(filename);
		m_isCompressed = false;
	} else {
		m_file = new KCompressionDevice(filename, ct);
		m_isCompressed = true;
	}
}

Reader::Reader(const QString &filename, QIODevice *file, bool autoclose, QObject *parent)
	: QObject(parent), m_filename(filename), m_file(file), m_current(-1), m_autoclose(autoclose), m_eof(false), m_isHibernation(false), m_isCompressed(false)
{
	Q_ASSERT(file);
}

Reader::~Reader()
{
	if(m_autoclose)
		delete m_file;
}

Compatibility Reader::open()
{
	if(!m_file->isOpen()) {
		if(!m_file->open(QFile::ReadOnly)) {
			return CANNOT_READ;
		}
	}

	// Read magic bytes "DPREC\0"
	char buf[6];
	if(m_file->read(buf, 6) != 6)
		return CANNOT_READ;

	if(memcmp(buf, "DPREC", 6) != 0) {
		// This may be a pre 2.0 recording
		if(memcmp(buf, "DPRECR", 6)==0)
			return INCOMPATIBLE;

		return NOT_DPREC;
	}

	// Read metadata block
	if(m_file->read(buf, 2) != 2)
		return NOT_DPREC;
	const quint16 metadatalen = qFromBigEndian<quint16>((const uchar*)buf);

	QByteArray metadatabuf = m_file->read(metadatalen);
	if(metadatabuf.length() != metadatalen)
		return NOT_DPREC;

	QJsonParseError jsonError;
	QJsonDocument metadatadoc = QJsonDocument::fromJson(metadatabuf, &jsonError);

	if(jsonError.error != QJsonParseError::NoError) {
		qWarning() << jsonError.errorString();
		return NOT_DPREC;
	}

	m_metadata = metadatadoc.object();

	// Header completed!
	m_beginning = m_file->pos();

	// Check version numbers
	m_version = protocol::ProtocolVersion::fromString(m_metadata["version"].toString());

	// Best case is exact match.
	const protocol::ProtocolVersion current = protocol::ProtocolVersion::current();
	if(m_version == current)
		return COMPATIBLE;

	// Different namespace means this recording is meant for some other program
	if(m_version.ns() != current.ns())
		return NOT_DPREC;

	// A recording made with a newer (major) version may contain unsupported commands.
	if(current.major() < m_version.major())
		return UNKNOWN_COMPATIBILITY;

	// Newer minor version: expect rendering differences
	if(current.minor() < m_version.minor())
		return MINOR_INCOMPATIBILITY;

#if 0
#if DRAWPILE_PROTO_MAJOR_VERSION != 16 || DRAWPILE_PROTO_MINOR_VERSION != 1
#error Update recording compatability check!
#endif

	// Old versions known to be compatible
	switch(m_formatversion) {
	case version32(15, 5): // fully compatible (with support code)
	case version32(14, 5):
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
#endif

	// Other versions are not supported
	return INCOMPATIBLE;
}

QString Reader::errorString() const
{
	return m_file->errorString();
}

QString Reader::writerVersion() const
{
	return m_metadata["writerversion"].toString();
}

qint64 Reader::filesize() const
{
	return m_file->size();
}

qint64 Reader::filePosition() const
{
	return m_file->pos();
}

void Reader::close()
{
	Q_ASSERT(m_file->isOpen());
	m_file->close();
}

void Reader::rewind()
{
	m_file->seek(m_beginning);
	m_current = -1;
	m_currentPos = -1;
	m_eof = false;
}

void Reader::seekTo(int pos, qint64 position)
{
	m_current = pos;
	m_currentPos = position;
	m_file->seek(position);
	m_eof =false;
}

bool Reader::readNextToBuffer(QByteArray &buffer)
{
	// Read length and type header
	if(buffer.length() < protocol::Message::HEADER_LEN)
		buffer.resize(1024);

	m_currentPos = filePosition();

	if(m_file->read(buffer.data(), protocol::Message::HEADER_LEN) != protocol::Message::HEADER_LEN) {
		m_eof = true;
		return false;
	}

	const int len = protocol::Message::sniffLength(buffer.constData());

	if(buffer.length() < len)
		buffer.resize(len);

	// Read message payload
	const int payloadlen = len - protocol::Message::HEADER_LEN;
	if(m_file->read(buffer.data()+protocol::Message::HEADER_LEN, payloadlen) != payloadlen) {
		m_eof = true;
		return false;
	}

	++m_current;

	return true;
}

MessageRecord Reader::readNext()
{
	MessageRecord msg;

	if(!readNextToBuffer(m_msgbuf))
		return msg;

	protocol::Message *message;
#if 0 // TODO
	if(m_formatversion != version32(DRAWPILE_PROTO_MAJOR_VERSION, DRAWPILE_PROTO_MINOR_VERSION)) {


		// see protocol changelog in doc/protocol.md
		switch(_formatversion) {
		case version32(15, 5):
			message = compat::deserializeV15_5((const uchar*)_msgbuf.constData(), _msgbuf.length());
			break;

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

		qWarning("TODO: recording compatability mode not yet implemented!");
		message = 0;
	} else {
		message = protocol::Message::deserialize((const uchar*)m_msgbuf.constData(), m_msgbuf.length(), true);
	}
#endif
	message = protocol::Message::deserialize((const uchar*)m_msgbuf.constData(), m_msgbuf.length(), true);

	if(message) {
		msg.status = MessageRecord::OK;
		msg.message = message;
	} else {
		msg.status = MessageRecord::INVALID;
		msg.error.len = protocol::Message::sniffLength(m_msgbuf.constData());
		msg.error.type = protocol::MessageType(m_msgbuf.at(2));
	}

	return msg;
}

}
