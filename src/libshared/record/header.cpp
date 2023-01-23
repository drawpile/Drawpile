/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#include "config.h"
#include "libshared/record/header.h"
#include "libshared/net/protover.h"
#include "libshared/net/message.h"

#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>
#include <QDebug>

#include <cstring>

namespace recording {

QJsonObject readRecordingHeader(QIODevice *file)
{
	Q_ASSERT(file && file->isOpen());

	// Read magic bytes "DPREC\0"
	char buf[6];
	if(file->read(buf, 6) != 6)
		return QJsonObject();

	if(memcmp(buf, "DPREC", 6) != 0)
		return QJsonObject();

	// Read metadata block
	if(file->read(buf, 2) != 2)
		return QJsonObject();

	const quint16 metadatalen = qFromBigEndian<quint16>((const uchar*)buf);

	QByteArray metadatabuf = file->read(metadatalen);
	if(metadatabuf.length() != metadatalen)
		return QJsonObject();

	QJsonParseError jsonError;
	QJsonDocument doc = QJsonDocument::fromJson(metadatabuf, &jsonError);

	if(jsonError.error != QJsonParseError::NoError) {
		qWarning() << jsonError.errorString();
		return QJsonObject();
	}

	if(!doc.isObject()) {
		qWarning() << "Recording metadata didn't contain an object!";
		return QJsonObject();
	}

	return doc.object();
}

static QJsonObject withDefaults(const QJsonObject metadata)
{
	QJsonObject header = metadata;
	if(!header.contains("version"))
		header["version"] = protocol::ProtocolVersion::current().asString();

	if(!header.contains("writerversion"))
		header["writerversion"] = DRAWPILE_VERSION;

	return header;
}

bool writeRecordingHeader(QIODevice *file, const QJsonObject &metadata)
{
	Q_ASSERT(file && file->isOpen());

	// Format identification
	const char *MAGIC = "DPREC";
	file->write(MAGIC, 6);

	// Metadata block
	QJsonObject md = withDefaults(metadata);

	QByteArray metadatabuf = QJsonDocument(md).toJson(QJsonDocument::Compact);

	if(metadatabuf.length() > 0xffff) {
		qWarning("Recording metadata block too long (%lld)", qlonglong(metadatabuf.length()));
		return false;
	}

	uchar lenbuf[2];
	qToBigEndian(quint16(metadatabuf.length()), lenbuf);

	if(file->write((const char*)lenbuf, 2) != 2)
		return false;

	if(file->write(metadatabuf) != metadatabuf.length())
		return false;

	return true;
}

bool writeTextHeader(QIODevice *file, const QJsonObject &metadata)
{
	Q_ASSERT(file && file->isOpen());
	QJsonObject md = withDefaults(metadata);
	QMapIterator<QString,QVariant> i(md.toVariantMap());
	while(i.hasNext()) {
		i.next();
		QByteArray line = QString("!%1=%2\n").arg(i.key(), i.value().toString()).toUtf8();
		if(file->write(line) != line.length())
			return false;
	}
	return file->write("\n", 1) == 1;
}

bool readRecordingMessage(QIODevice *file, QByteArray &buffer)
{
	Q_ASSERT(file && file->isOpen());

	// Read length and type header
	if(buffer.length() < protocol::Message::HEADER_LEN)
		buffer.resize(128);

	if(file->read(buffer.data(), protocol::Message::HEADER_LEN) != protocol::Message::HEADER_LEN)
		return false;

	const int len = protocol::Message::sniffLength(buffer.constData());

	if(buffer.length() < len)
		buffer.resize(len);

	// Read message payload
	const int payloadlen = len - protocol::Message::HEADER_LEN;
	if(file->read(buffer.data()+protocol::Message::HEADER_LEN, payloadlen) != payloadlen)
		return false;

	return true;
}

int skipRecordingMessage(QIODevice *file, uint8_t *msgType, uint8_t *ctxId)
{
	Q_ASSERT(file && file->isOpen());
	Q_ASSERT(!file->isSequential()); // skipping in sequential streams not currently needed

	char header[protocol::Message::HEADER_LEN];

	if(file->read(header, protocol::Message::HEADER_LEN) != sizeof(protocol::Message::HEADER_LEN))
		return -1;

	const int payloadLen = qFromBigEndian<quint16>((uchar*)header);

	const qint64 target = file->pos() + payloadLen;
	if(target > file->size() || !file->seek(target))
		return -1;

	if(msgType)
		*msgType = header[2];
	if(ctxId)
		*ctxId = header[3];

	return protocol::Message::HEADER_LEN + payloadLen;
}

}

