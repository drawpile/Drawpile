/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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

#ifndef DP_SHARED_NET_MSGSTREAM_H
#define DP_SHARED_NET_MSGSTREAM_H

#include <QList>

#include "message.h"

namespace protocol {

/**
 * @brief The ordered stream of command messages
 *
 */
class MessageStream {
public:
	MessageStream();

	/**
	 * @brief Get the current stream offset
	 *
	 * The indexes of the messages in the stream should not change
	 * even if older parts of it are discarded to save memory.
	 *
	 * @return index of first stored message
	 */
	int offset() const { return _offset; }

	/**
	 * @brief Get the end index of the stream
	 * @return
	 */
	int end() const { return _offset + _messages.size(); }

	/**
	 * @brief Check if a message at the given index exists in this stream
	 * @param i
	 * @return true if message can be got with at(i)
	 */
	bool isValidIndex(int i) const { return i >= offset() && i < end(); }

	MessagePtr at(int pos) { return _messages.at(pos-_offset); }

	const MessagePtr at(int pos) const { return _messages.at(pos-_offset); }

	/**
	 * @brief Add a new command to the stream
	 * @param msg command to add
	 */
	void append(MessagePtr msg) { _messages.append(msg); }

	/**
	 * @brief Create a new snapshot point
	 * @pre previous snapshot point must be complete
	 */
	void addSnapshotPoint();

	/**
	 * @brief Does this stream contain a snapshot
	 *
	 * @return true if this stream contains a snapshot
	 */
	bool hasSnapshot() const { return isValidIndex(_snapshotpointer); }

	/**
	 * @brief Get the latest snapshot point
	 * @return snapshot point
	 * @pre hasSnapshot() == true
	 */
	MessagePtr snapshotPoint() const { return at(_snapshotpointer); }

	/**
	 * @brief Get the index to the snapshot point
	 * @return snapshot point index
	 * @pre hasSnapshot() == true
	 */
	int snapshotPointIndex() const { return _snapshotpointer; }

	/**
	 * @brief remove all messages before the last complete snapshot point
	 * @return the number of messages removed
	 */
	int cleanup();

	/**
	 * @brief remove all messages, including the snapshot point
	 */
	void clear();

	/**
	 * @brief return the whole stream as a list
	 * @return list of messages
	 */
	QList<MessagePtr> toList() const { return _messages; }

	/**
	 * @brief return a filtered copy of the stream as a list, containing only the command stream messages.
	 * @return command stream messages
	 */
	QList<MessagePtr> toCommandList() const;

private:
	QList<MessagePtr> _messages;
	int _offset;
	int _snapshotpointer;
};

}

#endif

