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
#ifndef WRITER_H
#define WRITER_H

#include "libshared/net/message.h"

#include <QObject>
#include <QJsonObject>

class QIODevice;
class QTimer;

namespace recording {

class Writer final : public QObject
{
	Q_OBJECT
public:
	enum class Encoding {
		Binary,
		Text
	};

	/**
	 * @brief Open a writer that writes to the named file
	 *
	 * If the file ends with ".dptxt", the text encoding is used.
	 *
	 * @param filename
	 * @param parent
	 */
	Writer(const QString &filename, QObject *parent=nullptr);

	/**
	 * @brief Open a writer that writes to the given device
	 * @param file file device
	 * @param autoclose if true, this object will take ownership of the file device
	 * @param parent
	 */
	Writer(QIODevice *file, bool autoclose=false, QObject *parent=nullptr);
	~Writer() override;

	QString errorString() const;

	/**
	 * @brief Select the recording encoding to use.
	 *
	 * This must be called before any write operation.
	 */
	void setEncoding(Encoding e);

	//! Open the file for writing
	bool open();

	//! Close the file
	void close();

	//! Enable periodic flushing of the output file
	void setAutoflush();

	/**
	 * @brief Set the minimum time between messages before writing an Interval message
	 *
	 * Set the time to 0 to disable interval messages altogether
	 *
	 * @param min minimum time in milliseconds (0 to disable)
	 */
	void setMinimumInterval(int min);

	/**
	 * @brief Set the interval between timestamp markers.
	 * If set to zero, timestamps are not recorded.
	 * @param interval in milliseconds (0 to disable)
	 */
	void setTimestampInterval(int interval);

	/**
	 * @brief Write recording header
	 *
	 * This should be called before writing the first message.
	 *
	 * Custom metadata can be included. If no "version" field is set,
	 * the current protocol version will be used.
	 *
	 * @return false on error
	 */
	bool writeHeader(const QJsonObject &customMetadata=QJsonObject());

	/**
	 * @brief Write a message from a buffer
	 *
	 * Note. The buffer must contain a valid serialized Message!
	 * @param buffer
	 */
	void writeFromBuffer(const QByteArray &buffer);

	/**
	 * @brief Write a message
	 * @return false on error
	 */
	bool writeMessage(const protocol::Message &msg);

	/**
	 * @brief Write a comment line
	 *
	 * If encoding is not Text, this does nothing.
	 *
	 * @return false on error
	 */
	bool writeComment(const QString &comment);

public slots:
	/**
	 * @brief Record a message
	 *
	 * This writes the message to the recording only if the type is recordable.
	 * If a minimum interval is set and enough time has passed
	 * since the last recordMessage call, an Interval message is writen as well.
	 */
	void recordMessage(const protocol::MessagePtr &msg);

private:
	QIODevice *m_file;
	bool m_autoclose;
	qint64 m_minInterval;
	qint64 m_interval;
	qint64 m_timestampInterval;
	qint64 m_lastTimestamp;
	QTimer *m_autoflush;
	Encoding m_encoding;
};

}

#endif
