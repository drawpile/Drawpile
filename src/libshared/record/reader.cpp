/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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

#include "libshared/record/reader.h"
#include "libshared/record/header.h"
#include "libshared/net/recording.h"
#include "libshared/net/textmode.h"

#include "config.h"

#include <QtEndian>
#include <QVarLengthArray>
#include <QFile>
#include <QRegularExpression>

namespace recording {

using protocol::text::Parser;

struct Reader::Private {
	Encoding encoding;
	QString filename;
	QIODevice *file;

	QByteArray msgbuf;

	QJsonObject metadata;

	int current;
	qint64 currentPos;
	qint64 beginning;

	bool autoclose;
	bool eof;
	bool isCompressed;
	bool opaque;
};

bool Reader::isRecordingExtension(const QString &filename)
{
	QRegularExpression re("\\.dp(?:rec|txt)(?:z|\\.(?:gz|bz2|xz))?$");
	return re.match(filename).hasMatch();
}

Reader::Reader(const QString &filename, Encoding encoding, QObject *parent)
	: QObject(parent), d(new Private)
{
	d->encoding = encoding;
	d->filename = filename;
	d->current = -1;
	d->currentPos = 0;
	d->autoclose = true;
	d->eof = false;
	d->opaque = false;
	d->file = new QFile(filename);
}

Reader::Reader(const QString &filename, QIODevice *file, bool autoclose, Encoding encoding, QObject *parent)
	: QObject(parent), d(new Private)
{
	Q_ASSERT(file);
	d->encoding = encoding;
	d->filename = filename;
	d->file = file;
	d->current = -1;
	d->autoclose = autoclose;
	d->eof = false;
	d->isCompressed = false;
}

Reader::~Reader()
{
	if(d->autoclose)
		delete d->file;
	delete d;
}

bool Reader::isEof() const
{
	return d->eof;
}

protocol::ProtocolVersion Reader::formatVersion() const
{
	return protocol::ProtocolVersion::fromString(d->metadata["version"].toString());
}

QJsonObject Reader::metadata() const
{
	return d->metadata;
}

Reader::Encoding Reader::encoding() const
{
	return d->encoding;
}

QString Reader::filename() const
{
	return d->filename;
}

int Reader::currentIndex() const
{
	return d->current;
}

qint64 Reader::currentPosition() const
{
	return d->currentPos;
}

static Reader::Encoding detectEncoding(QIODevice *dev)
{
	// First, see if the binary header is present
	QJsonObject header = readRecordingHeader(dev);
	if(!header.isEmpty()) {
		// Header content read! This must be a binary recording
		return Reader::Encoding::Binary;
	}

	dev->seek(0);

	// Check if this looks like a text mode recording
	dev->setTextModeEnabled(true);
	Parser parser;
	for(int i=0;i<100;++i) {
		QByteArray rawLine = dev->readLine(1024);
		if(rawLine.isEmpty()) {
			// Ran out of stuff, certainly not a valid recording
			break;
		}

		QString line = QString::fromUtf8(rawLine).trimmed();
		Parser::Result res = parser.parseLine(line);
		switch(res.status) {
		case Parser::Result::Ok:
		case Parser::Result::NeedMore:
			// Got a valid command: this really looks like a valid recording
			return Reader::Encoding::Text;
			break;
		case Parser::Result::Skip:
			// Hmm. Inconclusive, unless a metadata variable was set
			if(!parser.metadata().isEmpty())
				return Reader::Encoding::Text;
			break;
		case Parser::Result::Error:
			// Error encountered: most likely not a recording
			return Reader::Encoding::Autodetect;
		}
	}
	// No valid messages after 100 lines? Probably not a recording.
	return Reader::Encoding::Autodetect;
}


Compatibility Reader::open()
{
	return open(false);
}

Compatibility Reader::openOpaque()
{
	return open(true);
}

Compatibility Reader::open(bool opaque)
{
	if(!d->file->isOpen()) {
		if(!d->file->open(QFile::ReadOnly)) {
			return CANNOT_READ;
		}
	}

	if(d->encoding == Encoding::Autodetect) {
		d->encoding = detectEncoding(d->file);
		// Still couldn't figure out the encoding?
		if(d->encoding == Encoding::Autodetect)
			return NOT_DPREC;

		d->file->seek(0);
	}

	d->opaque = opaque;

	if(d->encoding == Encoding::Binary)
		return readBinaryHeader();
	else
		return readTextHeader();
}

Compatibility Reader::readBinaryHeader() {
	// Read the header
	d->metadata = readRecordingHeader(d->file);

	if(d->metadata.isEmpty()) {
		return NOT_DPREC;
	}

	// Header completed!
	d->beginning = d->file->pos();

	// Check version numbers
	const auto version = formatVersion();

	// Best case is exact match.
	const auto current = protocol::ProtocolVersion::current();

	if(version == current)
		return COMPATIBLE;

	if(d->opaque) {
		// In opaque mode, it's enough that the server version matches
		if(version.serverVersion() == current.serverVersion())
			return COMPATIBLE;

	} else {
		// Different namespace means this recording is meant for some other program
		if(version.ns() != current.ns())
			return NOT_DPREC;

		// Backwards compatible mode:
		// TODO

		// Strict compatibility mode:

		// A recording made with a newer (major) version may contain unsupported commands.
		if(current.majorVersion() < version.majorVersion())
			return UNKNOWN_COMPATIBILITY;

		// Versions older than 21.x are known to be incompatible
		if(version.majorVersion() < 21)
			return INCOMPATIBLE;

		// Different minor version: expect rendering differences
		if(current.minorVersion() != version.minorVersion())
			return MINOR_INCOMPATIBILITY;
	}

	// Other versions are not supported
	return INCOMPATIBLE;
}

Compatibility Reader::readTextHeader()
{
	// Read the file until the first command is found
	Parser parser;
	bool done=false;
	qint64 pos=0;
	while(!done) {
		QByteArray rawLine = d->file->readLine();
		if(rawLine.isEmpty())
			return NOT_DPREC;

		QString line = QString::fromUtf8(rawLine).trimmed();
		Parser::Result res = parser.parseLine(line);
		switch(res.status) {
		case Parser::Result::Skip:
			// Comments or metadata. Remember this potential start of the first real message
			pos = filePosition();
			break;

		case Parser::Result::Error:
			return NOT_DPREC;

		case Parser::Result::NeedMore:
		case Parser::Result::Ok:
			// First message found, meaning the header section (if there was any) is over
			done = true;
			break;
		}
	}

	// Go to the beginning of the first message
	d->beginning = pos;
	d->file->seek(pos);

	// Convert header metadata to JSON format
	protocol::KwargsIterator header(parser.metadata());
	while(header.hasNext()) {
		header.next();
		bool ok;
		int number = header.value().toInt(&ok);
		if(ok)
			d->metadata[header.key()] = number;
		else if(header.value() == "true" || header.value() == "false")
			d->metadata[header.key()] = header.value() == "true";
		else
			d->metadata[header.key()] = header.value();
	}

	// Check compatibility
	const auto version = formatVersion();

	if(!version.isValid()) {
		// No version header given
		return UNKNOWN_COMPATIBILITY;
	}

	// Best case is exact match.
	const auto current = protocol::ProtocolVersion::current();

	if(version == current)
		return COMPATIBLE;

	if(d->opaque) {
		// In opaque mode, version must match exactly, except for the minor number
		if(version.ns() == current.ns() && version.serverVersion() == current.serverVersion() && version.majorVersion() == current.majorVersion())
			return COMPATIBLE;

	} else {
		// Different namespace means this recording is meant for some other program
		if(version.ns() != current.ns())
			return NOT_DPREC;

		// Backwards compatible mode:
		// TODO

		// Strict compatibilty mode:
		// A recording made with a newer (major) version may contain unsupported commands.
		if(current.majorVersion() < version.majorVersion())
			return UNKNOWN_COMPATIBILITY;

		// Versions older than 20.x are known to be incompatible
		if(version.majorVersion() < 20)
			return INCOMPATIBLE;

		// Different minor version: expect rendering differences
		if(current.minorVersion() != version.minorVersion())
			return MINOR_INCOMPATIBILITY;
	}

	// Other versions are not supported
	return INCOMPATIBLE;
}

QString Reader::errorString() const
{
	return d->file->errorString();
}

QString Reader::writerVersion() const
{
	return d->metadata["writerversion"].toString();
}

qint64 Reader::filesize() const
{
	return d->file->size();
}

qint64 Reader::filePosition() const
{
	return d->file->pos();
}

void Reader::close()
{
	Q_ASSERT(d->file->isOpen());
	d->file->close();
}

void Reader::rewind()
{
	d->file->seek(d->beginning);
	d->current = -1;
	d->currentPos = -1;
	d->eof = false;
}

void Reader::seekTo(int pos, qint64 position)
{
	d->current = pos;
	d->currentPos = position;
	d->file->seek(position);
	d->eof = false;
}

static protocol::NullableMessageRef readTextMessage(QIODevice *file, bool *eof)
{
	Parser parser;
	while(1) {
		QByteArray rawLine = file->readLine();
		if(rawLine.isEmpty()) {
			*eof = true;
			return nullptr;
		}

		Parser::Result res = parser.parseLine(QString::fromUtf8(rawLine).trimmed());
		switch(res.status) {
		case Parser::Result::Skip:
		case Parser::Result::NeedMore:
			break;
		case Parser::Result::Error:
			qWarning("Text mode recording error: %s", parser.errorString().toLocal8Bit().constData());
			return nullptr;
		case Parser::Result::Ok:
			return res.msg;
		}
	}
}

bool Reader::readNextToBuffer(QByteArray &buffer)
{
	Q_ASSERT(d->encoding != Encoding::Autodetect);

	d->currentPos = filePosition();

	if(d->encoding == Encoding::Binary) {
		if(!readRecordingMessage(d->file, buffer)) {
			d->eof = true;
			return false;
		}

	} else {
		protocol::NullableMessageRef msg = readTextMessage(d->file, &d->eof);
		if(msg.isNull())
			return false;
		if(buffer.length() < msg->length())
			buffer.resize(msg->length());
		msg->serialize(buffer.data());
	}

	++d->current;

	return true;
}

MessageRecord Reader::readNext()
{
	Q_ASSERT(d->encoding != Encoding::Autodetect);

	if(d->encoding == Encoding::Binary) {
		if(!readNextToBuffer(d->msgbuf))
			return MessageRecord::Eor();

		protocol::NullableMessageRef message;
		message = protocol::Message::deserialize((const uchar*)d->msgbuf.constData(), d->msgbuf.length(), !d->opaque);

		if(message.isNull())
			return MessageRecord::Invalid(
				protocol::Message::sniffLength(d->msgbuf.constData()),
				protocol::MessageType(d->msgbuf.at(2))
			);
		else
			return MessageRecord::Ok(message);

	} else {
		d->currentPos = filePosition();
		protocol::NullableMessageRef message = readTextMessage(d->file, &d->eof);
		if(!d->eof) {
			if(message.isNull())
				return MessageRecord::Invalid(0, protocol::MSG_COMMAND);

			++d->current;
			return MessageRecord::Ok(message);
		}
	}

	return MessageRecord::Eor();
}

}

