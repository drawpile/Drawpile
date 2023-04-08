// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_REC_HEADER_H
#define DP_REC_HEADER_H

#include <cstdint>

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
 * @brief Write the text mode recording header
 */
bool writeTextHeader(QIODevice *file, const QJsonObject &metadata);
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

/**
 * @brief Skip a single message in the recording
 * @param file
 * @param msgType
 * @param ctxId
 * @return length of the message skipped or -1 if end whole message is not in the file
 */
int skipRecordingMessage(QIODevice *file, uint8_t *msgType=nullptr, uint8_t *ctxId=nullptr);

}

#endif

