/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2019 Calle Laakkonen

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

#ifndef CANVAS_HISTORY_H
#define CANVAS_HISTORY_H

#include <QList>

#include "../shared/net/message.h"

namespace canvas {

/**
 * @brief Sequence of messages to reproduce the canvas state
 *
 * The whole session history might not be in memory, but it
 * should always contain enough messages to reach
 * end of the Undo history.
 */
class History {
public:
	History();

	/**
	 * @brief Get the current stream offset
	 *
	 * The indexes of the messages in the stream should not change
	 * even if older parts of it are discarded to save memory.
	 *
	 * @return index of first stored message
	 */
	int offset() const { return m_offset; }

	/**
	 * @brief Get the end index of the stream
	 * @return
	 */
	int end() const { return m_offset + m_messages.size(); }

	/**
	 * @brief Check if a message at the given index is in memory
	 * @param i
	 * @return true if message can be got with at(i)
	 */
	bool isValidIndex(int i) const { return i >= offset() && i < end(); }

	/**
	 * @brief at Get the message at the given index
	 */
	protocol::MessagePtr at(int pos) const { return m_messages.at(pos-m_offset); }

	/**
	 * @brief Add a new command to the stream
	 * @param msg command to add
	 */
	void append(protocol::MessagePtr msg);

	/**
	 * @brief Delete old messages until the given index
	 */
	void cleanup(int until);

	/**
	 * @brief Remove all messages and change the offset
	 *
	 * @param newoffset
	 */
	void resetTo(int newoffset);

	/**
	 * @brief Get the length of the stored message stream in bytes.
	 *
	 * This returns the length of the serialized messages, not the in-memory
	 * representation.
	 * @return (serialized) length in bytes
	 */
	uint lengthInBytes() const { return m_bytes; }

	/**
	 * @brief return the whole stream as a list
	 * @return list of messages
	 */
	protocol::MessageList toList() const { return m_messages; }

private:
	protocol::MessageList m_messages;
	int m_offset;
	uint m_bytes;
};

}

#endif

