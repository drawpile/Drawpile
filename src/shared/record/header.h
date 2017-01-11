/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#ifndef DP_REC_HEADER_H
#define DP_REC_HEADER_H

class QIODevice;
class QByteArray;
class QJsonObject;

namespace recording {

/**
 * @brief Read a recording header
 *
 * A null object is returned if the header couldn't be read
 * or the file is not a valid Drawpile recording.
 * @param file
 * @return header metadata block
 */
QJsonObject readRecordingHeader(QIODevice *file);

/**
 * @brief Write a recording header
 *
 * The keys "version" and "writerversion" will be automatically
 * set if not present in the given metadata.
 *
 * @param file
 * @param metadata header metadata
 * @return false on IO error
 */
bool writeRecordingHeader(QIODevice *file, const QJsonObject &metadata);

/**
 * @brief Read a single complete message from a file to the buffer
 *
 * The target buffer is resized to fit the message if necessary.
 *
 * @param file
 * @param buffer
 * @return false on IO error
 */
bool readRecordingMessage(QIODevice *file, QByteArray &buffer);

}

#endif

