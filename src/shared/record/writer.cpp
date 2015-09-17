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

#include "writer.h"
#include "util.h"
#include "../net/recording.h"

#include "config.h"

#include <QVarLengthArray>
#include <QDateTime>
#include <QtEndian>
#include <QSaveFile>

#include <KCompressionDevice>
#include <QJsonObject>
#include <QJsonDocument>

namespace recording {

Writer::Writer(const QString &filename, QObject *parent)
	: Writer(new QSaveFile(filename), true, parent)
{
	KCompressionDevice::CompressionType ct = KCompressionDevice::None;
	if(filename.endsWith(".gz", Qt::CaseInsensitive) || filename.endsWith(".dprecz", Qt::CaseInsensitive))
		ct = KCompressionDevice::GZip;
	else if(filename.endsWith(".bz2", Qt::CaseInsensitive))
		ct = KCompressionDevice::BZip2;
	else if(filename.endsWith(".xz", Qt::CaseInsensitive))
		ct = KCompressionDevice::Xz;

	m_savefile = static_cast<QSaveFile*>(m_file);
	if(ct != KCompressionDevice::None)
		m_file = new KCompressionDevice(m_savefile, true, ct);
}


Writer::Writer(QIODevice *file, bool autoclose, QObject *parent)
	: QObject(parent), m_file(file),
	m_savefile(nullptr),
	m_autoclose(autoclose), m_minInterval(0)
{
}

Writer::~Writer()
{
	if(m_autoclose)
		delete m_file;
}

void Writer::setMinimumInterval(int min)
{
	m_minInterval = min;
	m_interval = QDateTime::currentMSecsSinceEpoch();
}

bool Writer::open()
{
	if(m_file->isOpen())
		return true;

	// Open savefile explicitly, because otherwise the compression filter
	// will open&close it for us. We need to call commit() before savefile is
	// closed but after the compressor is finished.
	if(m_savefile && m_savefile != m_file) {
		if(!m_savefile->open(QIODevice::WriteOnly))
			return false;
	}

	return m_file->open(QIODevice::WriteOnly);
}

QString Writer::errorString() const
{
	return m_file->errorString();
}

bool Writer::writeHeader()
{
	Q_ASSERT(m_file->isOpen());

	// Format identification
	const char *MAGIC = "DPREC";
	m_file->write(MAGIC, 6);

	// Metadata block
	QJsonObject metadata;
	QJsonObject version;
	version["major"] = DRAWPILE_PROTO_MAJOR_VERSION;
	version["minor"] = DRAWPILE_PROTO_MINOR_VERSION;
	version["str"] = DRAWPILE_VERSION;
	metadata["version"] = version;

	// Write metadata
	QByteArray metadatabuf = QJsonDocument(metadata).toJson(QJsonDocument::Compact);

	uchar lenbuf[2];
	qToBigEndian(quint16(metadatabuf.length()), lenbuf);
	m_file->write((const char*)lenbuf, 2);

	m_file->write(metadatabuf);

	return true;
}

void Writer::writeFromBuffer(const QByteArray &buffer)
{
	int len = protocol::Message::sniffLength(buffer.constData());
	Q_ASSERT(len <= buffer.length());
	m_file->write(buffer.constData(), len);
}

void Writer::writeMessage(const protocol::Message &msg)
{
	Q_ASSERT(m_file->isOpen());

	if(msg.isRecordable()) {
		// Write Interval message if sufficient time has passed since last message was written
		if(m_minInterval>0) {
			qint64 now = QDateTime::currentMSecsSinceEpoch();
			qint64 interval = now - m_interval;
			if(interval >= m_minInterval) {
				protocol::Interval imsg(0, qMin(qint64(0xffff), interval));
				QVarLengthArray<char> ibuf(imsg.length());
				int ilen = imsg.serialize(ibuf.data());
				m_file->write(ibuf.data(), ilen);
			}
			m_interval = now;
		}

		// Write the actual message
		QVarLengthArray<char> buf(msg.length());
		int len = msg.serialize(buf.data());
		Q_ASSERT(len == buf.length());
		m_file->write(buf.data(), len);
	}
}


void Writer::recordMessage(const protocol::MessagePtr msg)
{
	writeMessage(*msg);
}

void Writer::close()
{
	if(m_file->isOpen()) {
		if(m_savefile) {
			// If file is not the same as savefile, it is the compression device.
			// We must close it first to ensure all buffers are flushed, then
			// commit the savefile.
			if(m_file != m_savefile)
				m_file->close();

			m_savefile->commit();

		} else {
			m_file->close();
		}
	}
}

}
