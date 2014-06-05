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
#ifndef WRITER_H
#define WRITER_H

#include <QObject>

#include "../net/message.h"

class QFileDevice;

namespace recording {

class HibernationHeader;

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
	Writer(QFileDevice *file, bool autoclose=false, QObject *parent=0);
	Writer(const Writer&) = delete;
	Writer &operator=(const Writer&) = delete;
	~Writer();

	QString errorString() const;

	//! Open the file for writing
	bool open();

	//! Close the file
	void close();

	/**
	 * @brief Set the minimum time between messages before writing an Interval message
	 *
	 * Set the time to 0 to disable interval messages altogether
	 *
	 * @param min minimum time in milliseconds (0 to disable)
	 */
	void setMinimumInterval(int min);

	/**
	 * @brief Enable/disable meta message filtering.
	 *
	 * This is enabled by default
	 * @param filter
	 */
	void setFilterMeta(bool filter);

	/**
	 * @brief Write recording header
	 *
	 * This should be called before writing the first message.
	 *
	 * @return false on error
	 */
	bool writeHeader();

	/**
	 * @brief Write session hibernation file header
	 *
	 * Use this instead of writeHeader when creating a session hibernation file.
	 * You should also call setFilterMeta(false) to make sure all messages are saved.
	 *
	 * @param header
	 * @return
	 */
	bool writeHibernationHeader(const HibernationHeader &header);

	/**
	 * @brief Write a message from a buffer
	 *
	 * Note. The buffer must contain a valid serialized Message!
	 * @param buffer
	 */
	void writeFromBuffer(const QByteArray &buffer);

	void recordMessage(const protocol::Message &msg);

public slots:
	void recordMessage(const protocol::MessagePtr msg);

private:
	QFileDevice *_file;
	bool _autoclose;
	qint64 _minInterval;
	qint64 _interval;
	bool _filterMeta;
};

}

#endif
