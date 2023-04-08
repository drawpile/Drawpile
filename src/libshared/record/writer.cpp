// SPDX-License-Identifier: GPL-3.0-or-later

#include "libshared/record/writer.h"
#include "libshared/record/header.h"
#include "libshared/net/recording.h"

#include <QVarLengthArray>
#include <QDateTime>
#include <QFile>
#include <QTimer>

#include <memory>

namespace recording {

Writer::Writer(const QString &filename, QObject *parent)
	: Writer(new QFile(filename), true, parent)
{
	if(filename.contains(".dptxt", Qt::CaseInsensitive) && !filename.contains(".dprec", Qt::CaseInsensitive))
		m_encoding = Encoding::Text;
}


Writer::Writer(QIODevice *file, bool autoclose, QObject *parent)
	: QObject(parent), m_file(file),
	m_autoclose(autoclose), m_minInterval(0), m_timestampInterval(0), m_lastTimestamp(0),
	m_autoflush(nullptr), m_encoding(Encoding::Binary)
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

void Writer::setTimestampInterval(int interval)
{
	m_timestampInterval = interval;
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

void Writer::setEncoding(Encoding e)
{
	Q_ASSERT(m_file->pos()==0);
	m_encoding = e;
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
	if(m_encoding == Encoding::Binary)
		return writeRecordingHeader(m_file, customMetadata);
	else
		return writeTextHeader(m_file, customMetadata);
}

void Writer::writeFromBuffer(const QByteArray &buffer)
{
	if(m_encoding == Encoding::Binary) {
		const int len = protocol::Message::sniffLength(buffer.constData());
		Q_ASSERT(len <= buffer.length());
		m_file->write(buffer.constData(), len);

	} else {
		protocol::NullableMessageRef msg = protocol::Message::deserialize(reinterpret_cast<const uchar*>(buffer.constData()), buffer.length(), true);
		m_file->write(msg->toString().toUtf8());
		m_file->write("\n", 1);
	}
}

bool Writer::writeMessage(const protocol::Message &msg)
{
	Q_ASSERT(m_file->isOpen());

	if(m_encoding == Encoding::Binary) {
		QVarLengthArray<char> buf(msg.length());
		const int len = msg.serialize(buf.data());
		Q_ASSERT(len == buf.length());
		if(m_file->write(buf.data(), len) != len)
			return false;

	} else {
		if(msg.type() == protocol::MSG_FILTERED) {
			// Special case: Filtered messages are
			// written as comments in the text format.
			const protocol::Filtered &fm = static_cast<const protocol::Filtered&>(msg);
			auto wrapped = fm.decodeWrapped();

			QString comment;
			if(wrapped.isNull()) {
				comment = QStringLiteral("FILTERED: undecodable message type #%1 of length %2")
					.arg(fm.wrappedType())
					.arg(fm.wrappedPayloadLength());

			} else {
				comment = QStringLiteral("FILTERED: ") + wrapped->toString();
			}

			return writeComment(comment);
		}

		QByteArray line = msg.toString().toUtf8();
		if(m_file->write(line) != line.length())
			return false;
		if(m_file->write("\n", 1) != 1)
			return false;

		// Write extra newlines after certain commands to give
		// the file some visual structure
		switch(msg.type()) {
		case protocol::MSG_UNDOPOINT:
			if(m_file->write("\n", 1) != 1)
				return false;
			break;
		default: break;
		}
	}

	return true;
}

bool Writer::writeComment(const QString &comment)
{
	if(m_encoding != Encoding::Text)
		return true;

	QList<QByteArray> lines = comment.toUtf8().split('\n');
	for(const QByteArray &line : lines) {
		if(m_file->write("# ", 2) != 2)
			return false;

		if(m_file->write(line) != line.length())
			return false;

		if(m_file->write("\n", 1) != 1)
			return false;
	}

	return true;
}

void Writer::recordMessage(const protocol::MessagePtr &msg)
{
	if(msg->isRecordable()) {
		const qint64 now = QDateTime::currentMSecsSinceEpoch();

		if(m_minInterval>0) {
			const qint64 interval = now - m_interval;
			if(interval >= m_minInterval) {
				writeMessage(protocol::Interval(0, qMin(qint64(0xffff), interval)));
			}
			m_interval = now;
		}

		if(m_timestampInterval > 0) {
			const qint64 interval = now - m_lastTimestamp;
			if(interval >= m_timestampInterval) {
				writeMessage(protocol::Marker(0, QDateTime::currentDateTime().toString()));
				m_lastTimestamp = now;
			}
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

