/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2017 Calle Laakkonen

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
#include "header.h"
#include "../net/recording.h"

#include <QVarLengthArray>
#include <QDateTime>
#include <QFile>
#include <QTimer>

#include <KCompressionDevice>

namespace recording {

Writer::Writer(const QString &filename, QObject *parent)
	: Writer(new QFile(filename), true, parent)
{
	KCompressionDevice::CompressionType ct = KCompressionDevice::None;
	if(filename.endsWith(".gz", Qt::CaseInsensitive) || filename.endsWith(".dprecz", Qt::CaseInsensitive))
		ct = KCompressionDevice::GZip;
	else if(filename.endsWith(".bz2", Qt::CaseInsensitive))
		ct = KCompressionDevice::BZip2;
	else if(filename.endsWith(".xz", Qt::CaseInsensitive))
		ct = KCompressionDevice::Xz;

	if(ct != KCompressionDevice::None)
		m_file = new KCompressionDevice(m_file, true, ct);
}


Writer::Writer(QIODevice *file, bool autoclose, QObject *parent)
	: QObject(parent), m_file(file),
	m_autoclose(autoclose), m_minInterval(0), m_autoflush(nullptr)
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

void Writer::setAutoflush()
{
	if(m_autoflush != nullptr)
		return;

	auto *fd = qobject_cast<QFileDevice*>(m_file);
	if(!fd) {
		qWarning("Cannot enable recording autoflush: output device not a QFileDevice");
		return;
	}

	m_autoflush = new QTimer(this);
	m_autoflush->setSingleShot(false);
	connect(m_autoflush, &QTimer::timeout, fd, &QFileDevice::flush);
	m_autoflush->start(5000);
}

bool Writer::open()
{
	if(m_file->isOpen())
		return true;

	return m_file->open(QIODevice::WriteOnly);
}

QString Writer::errorString() const
{
	return m_file->errorString();
}

bool Writer::writeHeader(const QJsonObject &customMetadata)
{
	return writeRecordingHeader(m_file, customMetadata);
}

void Writer::writeFromBuffer(const QByteArray &buffer)
{
	const int len = protocol::Message::sniffLength(buffer.constData());
	Q_ASSERT(len <= buffer.length());
	m_file->write(buffer.constData(), len);
}

void Writer::writeMessage(const protocol::Message &msg)
{
	Q_ASSERT(m_file->isOpen());

	QVarLengthArray<char> buf(msg.length());
	const int len = msg.serialize(buf.data());
	Q_ASSERT(len == buf.length());

	m_file->write(buf.data(), len);
}


void Writer::recordMessage(const protocol::MessagePtr &msg)
{
	if(msg->isRecordable()) {
		if(m_minInterval>0) {
			const qint64 now = QDateTime::currentMSecsSinceEpoch();
			const qint64 interval = now - m_interval;
			if(interval >= m_minInterval) {
				writeMessage(protocol::Interval(0, qMin(qint64(0xffff), interval)));
			}
			m_interval = now;
		}

		writeMessage(*msg);
	}
}

void Writer::close()
{
	if(m_autoflush) {
		m_autoflush->stop();
		delete m_autoflush;
		m_autoflush = nullptr;
	}

	if(m_file->isOpen())
		m_file->close();
}

}

