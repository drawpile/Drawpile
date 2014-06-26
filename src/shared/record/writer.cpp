/*
   DrawPile - a collaborative drawing program.

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

#include "writer.h"
#include "hibernate.h"
#include "util.h"
#include "../net/recording.h"

#include "config.h"

#include <QVarLengthArray>
#include <QDateTime>
#include <QtEndian>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
#include <QSaveFile>
#else
#include <QFile>
#define QSaveFile QFile
#define NO_QSAVEFILE
#endif

namespace recording {

Writer::Writer(const QString &filename, QObject *parent)
	: Writer(new QSaveFile(filename), true, parent)
{
}


Writer::Writer(QFileDevice *file, bool autoclose, QObject *parent)
	: QObject(parent), _file(file), _autoclose(autoclose), _minInterval(0), _filterMeta(true)
{
}

Writer::~Writer()
{
	if(_autoclose)
		delete _file;
}

void Writer::setMinimumInterval(int min)
{
	_minInterval = min;
	_interval = QDateTime::currentMSecsSinceEpoch();
}

void Writer::setFilterMeta(bool filter)
{
	_filterMeta = filter;
}

bool Writer::open()
{
	if(_file->isOpen())
		return true;

	return _file->open(QIODevice::WriteOnly);
}

QString Writer::errorString() const
{
	return _file->errorString();
}

bool Writer::writeHeader()
{
	Q_ASSERT(_file->isOpen());

	// Format identification
	const char *MAGIC = "DPRECR";
	_file->write(MAGIC, 7);

	// Protocol version
	uchar version[4];
	qToBigEndian(version32(DRAWPILE_PROTO_MAJOR_VERSION, DRAWPILE_PROTO_MINOR_VERSION), version);
	_file->write((const char*)version, 4);

	// Program version
	const char *VERSION = DRAWPILE_VERSION;
	_file->write(VERSION, qstrlen(VERSION)+1);

	return true;
}

bool Writer::writeHibernationHeader(const HibernationHeader &header)
{
	Q_ASSERT(_file->isOpen());

	// Format identification (note the 'H' ending)
	const char *MAGIC = "DPRECH";
	_file->write(MAGIC, 7);

	// Protocol version
	// Major version is always the same as the server version, but minor version
	// is taken from the session
	uchar version[4];
	qToBigEndian(version32(DRAWPILE_PROTO_MAJOR_VERSION, header.minorVersion), version);
	_file->write((const char*)version, 4);

	// Program version (this is always the server version)
	const char *VERSION = DRAWPILE_VERSION;
	_file->write(VERSION, qstrlen(VERSION)+1);

	// Hibernation format version
	char hver = 1;
	_file->write(&hver, 1);

	// Session title
	QByteArray title = header.title.toUtf8();
	uchar titlelen[2];
	qToBigEndian<quint16>(title.length(), titlelen);
	_file->write((const char*)titlelen, 2);
	_file->write(title);

	// Session founder name
	QByteArray founder = header.founder.toUtf8();
	Q_ASSERT(founder.length() < 256); // usernames should be limited to something way shorter than this
	uchar founderlen = qMin(founder.length(), 255);
	_file->putChar(founderlen);
	_file->write(founder);

	// Session flags
	char flags = header.flags;
	_file->write(&flags, 1);

	// Session password
	QByteArray password = header.password;
	uchar passwdlen[2];
	qToBigEndian<quint16>(password.length(), passwdlen);
	_file->write((const char*)passwdlen, 2);
	_file->write(password);

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
	case MSG_MOVEPOINTER:
	case MSG_MARKER:
		return true;
	default:
		return false;
	}
}
}

void Writer::writeFromBuffer(const QByteArray &buffer)
{
	int len = protocol::Message::sniffLength(buffer.constData());
	Q_ASSERT(len <= buffer.length());
	_file->write(buffer.constData(), len);
}

void Writer::recordMessage(const protocol::Message &msg)
{
	Q_ASSERT(_file->isOpen());

	if(!_filterMeta || msg.isCommand() || isRecordableMeta(msg.type())) {
		// Write Interval message if sufficient time has passed since last message was written
		if(_minInterval>0) {
			qint64 now = QDateTime::currentMSecsSinceEpoch();
			qint64 interval = now - _interval;
			if(interval >= _minInterval) {
				protocol::Interval imsg(qMin(qint64(0xffff), interval));
				QVarLengthArray<char> ibuf(imsg.length());
				int ilen = imsg.serialize(ibuf.data());
				_file->write(ibuf.data(), ilen);
			}
			_interval = now;
		}

		// Write the actual message
		QVarLengthArray<char> buf(msg.length());
		int len = msg.serialize(buf.data());
		Q_ASSERT(len == buf.length());
		_file->write(buf.data(), len);
	}
}


void Writer::recordMessage(const protocol::MessagePtr msg)
{
	recordMessage(*msg);
}

void Writer::close()
{
	if(_file->isOpen()) {
#ifndef NO_QSAVEFILE // Qt 5.0 compatibility
		QSaveFile *sf = qobject_cast<QSaveFile*>(_file);
		if(sf)
			sf->commit();
		else
			_file->close();
#else
		_file->close();
#endif
	}
}

}
