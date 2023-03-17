/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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

#include "libshared/net/message.h"
#include "libshared/net/protover.h"

#include <QObject>
#include <QJsonObject>

class QIODevice;

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
	static MessageRecord Ok(protocol::NullableMessageRef msg) { return MessageRecord { OK, msg, 0, protocol::MSG_COMMAND }; }
	static MessageRecord Invalid(int len, protocol::MessageType type) { return MessageRecord { INVALID, nullptr, len, type }; }
	static MessageRecord Eor() { return MessageRecord { END_OF_RECORDING, nullptr, 0, protocol::MSG_COMMAND }; }

	enum { OK, INVALID, END_OF_RECORDING } status;

	// The message (if status is OK)
	protocol::NullableMessageRef message;

	// These are set if status is INVALID
	int invalid_len;
	protocol::MessageType invalid_type;
};

/**
 * @brief Recording file reader
 *
 * Supports both binary and text encodings.
 */
class Reader final : public QObject
{
	Q_OBJECT
public:
	enum class Encoding {
		Autodetect,
		Binary,
		Text
	};

	/**
	 * @brief Read from a file
	 *
	 * Compression is handled transparently.
	 *
	 * @brief Reader
	 * @param filename
	 * @param parent
	 */
	explicit Reader(const QString &filename, Encoding encoding=Encoding::Autodetect, QObject *parent=nullptr);

	/**
	 * @brief Read from an IO device
	 *
	 * @param filename the original file name
	 * @param file input file device
	 * @param autoclose if true, the Reader instance will take ownership of the file device
	 * @param parent
	 */
	Reader(const QString &filename, QIODevice *file, bool autoclose=false, Encoding=Encoding::Autodetect, QObject *parent=nullptr);

	~Reader() override;

	/**
	 * @brief Check if the given filename has a .dprec(+compression type) extension
	 * @param filename
	 * @return true if file is probably a recording
	 */
	static bool isRecordingExtension(const QString &filename);

	//! Name of the currently open file
	QString filename() const;

	//! Size of the currently open file
	qint64 filesize() const;

	//! Index of the last read message
	int currentIndex() const;

	//! Position of the last read message in the file
	qint64 currentPosition() const;

	//! Position in the file (position of the next message to be read)
	qint64 filePosition() const;

	//! Did the last read hit the end of the file?
	bool isEof() const;

	//! Get the last error message
	QString errorString() const;

	//! Get the recording's protocol version
	protocol::ProtocolVersion formatVersion() const;

	//! Get the version number of the program that made the recording
	QString writerVersion() const;

	//! Get header metadata
	QJsonObject metadata() const;

	/**
	 * @brief Open the file
	 * @return compatibility level of the opened file
	 */
	Compatibility open();

	/**
	 * @brief Open the file in opaque reading mode
	 *
	 * In this mode, a binary file is either compatible or
	 * incompatible with no partial compatibility modes.
	 * All versions where the server version matches the
	 * reader's are considered fully compatible.
	 * OpaqueMessage class is used for all opaque message types.
	 *
	 * For text mode recordings, the version must match exactly, except
	 * for the minor number.
	 * Real message classes are returned for opaque messages.
	 */
	Compatibility openOpaque();

	/**
	 * @brief Get the recording encoding mode
	 */
	Encoding encoding() const;

	//! Close the file
	void close();

	//! Rewind to the first message
	void rewind();

	/**
	 * @brief Read the next message to the given buffer
	 *
	 * The buffer will be resized, if necesasry, to hold the entire message.
	 * If this is a text mode recording, the message will be serialized in the buffer.
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
	Compatibility open(bool opaque);
	Compatibility readBinaryHeader();
	Compatibility readTextHeader();

	struct Private;
	Private *d;

};

}

#endif
