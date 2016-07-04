/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2016 Calle Laakkonen

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

#include "../net/message.h"

#include <QObject>

class QIODevice;
class QTimer;

namespace recording {

class Writer : public QObject
{
	Q_OBJECT
public:
	/**
	 * @brief Open a writer that writes to the named file
	 * @param filename
	 * @param parent
	 */
	Writer(const QString &filename, QObject *parent=0);

	/**
	 * @brief Open a writer that writes to the given device
	 * @param file file device
	 * @param autoclose if true, this object will take ownership of the file device
	 * @param parent
	 */
	Writer(QIODevice *file, bool autoclose=false, QObject *parent=0);
	Writer(const Writer&) = delete;
	Writer &operator=(const Writer&) = delete;
	~Writer();

	QString errorString() const;

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
	 * @brief Write recording header
	 *
	 * This should be called before writing the first message.
	 *
	 * @return false on error
	 */
	bool writeHeader();

	/**
	 * @brief Write a message from a buffer
	 *
	 * Note. The buffer must contain a valid serialized Message!
	 * @param buffer
	 */
	void writeFromBuffer(const QByteArray &buffer);
	void writeMessage(const protocol::Message &msg);

public slots:
	void recordMessage(const protocol::MessagePtr msg);

private:
	QIODevice *m_file;
	bool m_autoclose;
	qint64 m_minInterval;
	qint64 m_interval;
	QTimer *m_autoflush;
};

}

#endif
