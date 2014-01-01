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
#ifndef REC_READER_H
#define REC_READER_H

#include <QObject>
#include <QFile>
#include "../net/message.h"

namespace recording {

enum Compatibility {
	//! Recording is fully compatible with current version
	COMPATIBLE,
	//! Minor changes: file will load but may appear different
	MINOR_INCOMPATIBILITY,
	//! Might be at least partially compatible. Unknown future version
	UNKNOWN_COMPATIBILITY,
	//! File is a recording, but known to be incompatible
	INCOMPATIBLE,
	//! File is not a recording at all
	NOT_DPREC,
	//! Couldn't read file due to IO error
	CANNOT_READ
};

struct MessageRecord {
	MessageRecord() : status(END_OF_RECORDING), message(0) {}

	enum { OK, INVALID, END_OF_RECORDING } status;
	union {
		protocol::Message *message;
		struct {
			int len;
			protocol::MessageType type;
		};
	};
};

class Reader : public QObject
{
	Q_OBJECT
public:
	Reader(const QString &filename, QObject *parent=0);
	~Reader();

	QString filename() const { return _file.fileName(); }

	qint64 filesize() const { return _file.size(); }

	qint64 position() const { return _file.pos(); }

	QString errorString() const;

	/**
	 * @brief Get the version number of the program that made the recording.
	 *
	 * The version number is available after opening the file.
	 * @return
	 */
	const QString writerVersion() const { return _writerversion; }

	/**
	 * @brief Open the file
	 * @return compatibility level of the opened file
	 */
	Compatibility open();

	/**
	 * @brief Close the file
	 */
	void close();

	/**
	 * @brief Read the next message
	 * @return
	 */
	MessageRecord readNext();

private:
	QFile _file;
	QString _writerversion;
};

}

#endif
