/*
   Drawpile - a collaborative drawing program.

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
#ifndef REC_READER_H
#define REC_READER_H

#include <QObject>

#include "hibernate.h"
#include "../net/message.h"

class QFileDevice;

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
	struct MessageRecordError {
		int len;
		protocol::MessageType type;
	};
	union {
		protocol::Message *message;
		MessageRecordError error;
	};
};

class Reader : public QObject
{
	Q_OBJECT
public:
	/**
	 * @brief Reader
	 * @param filename
	 * @param parent
	 */
	Reader(const QString &filename, QObject *parent=0);

	/**
	 * @brief Reader
	 *
	 * @param file input file device
	 * @param autoclose if true, the Reader instance will take ownership of the file device
	 * @param parent
	 */
	Reader(QFileDevice *file, bool autoclose=false, QObject *parent=0);

	Reader(const Reader &) = delete;
	Reader &operator=(const Reader &) = delete;
	~Reader();

	//! Name of the currently open file
	QString filename() const;

	//! Size of the currently open file
	qint64 filesize() const;

	//! Index of the last read message
	int currentIndex() const { return _current; }

	//! Position of the last read message in the file
	qint64 currentPosition() const { return _currentPos; }

	//! Position in the file (position of the next message to be read)
	qint64 filePosition() const;

	//! Did the last read hit the end of the file?
	bool isEof() const { return _eof; }

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

	//! Close the file
	void close();

	/**
	 * @brief Get the session hibernation header
	 * @pre isHibernation() == true
	 */
	const HibernationHeader &hibernationHeader() const { return _hibheader; }

	//! Is this a session hibernation file instead of a normal recording?
	bool isHibernation() const { return _isHibernation; }

	//! Rewind to the first message
	void rewind();

	/**
	 * @brief Read the next message to the given buffer
	 *
	 * The buffer will be resized, if necesasry, to hold the entire message.
	 *
	 * @param buffer
	 * @return false on error
	 */
	bool readNextToBuffer(QByteArray &buffer);

	/**
	 * @brief Read the next message
	 * @return
	 */
	MessageRecord readNext();

	/**
	 * @brief Seek to given position in the recording
	 *
	 * Calling this will reset the EOF flag.
	 *
	 * @param pos entry index
	 * @param offset entry offset in the file
	 */
	void seekTo(int pos, qint64 offset);

private:
	QFileDevice *_file;
	QByteArray _msgbuf;
	QString _writerversion;
	HibernationHeader _hibheader;
	int _current;
	qint64 _currentPos;
	qint64 _beginning;
	bool _autoclose;
	bool _eof;
	bool _isHibernation;
};

}

#endif
