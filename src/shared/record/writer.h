/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/
#ifndef WRITER_H
#define WRITER_H

#include <QObject>

#include "../net/message.h"

class QFileDevice;

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

public slots:
	void recordMessage(const protocol::MessagePtr msg);

private:
	QFileDevice *_file;
	bool _autoclose;
	qint64 _minInterval;
	qint64 _interval;
};

}

#endif
