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
	bool isValidIndex(int i) { return i >= offset() && i < end(); }

	MessagePtr at(int pos) { return _messages.at(pos-_offset); }

	const MessagePtr at(int pos) const { return _messages.at(pos-_offset); }

	/**
	 * @brief Add a new command to the stream
	 * @param msg command to add
	 */
	void append(MessagePtr msg) { _messages.append(msg); }

	/**
	 * @brief Does this stream contain a snapshot
	 *
	 * @todo always returns true because stream discarding is not yet implemented
	 * @return true if this stream contains a snapshot
	 */
	bool hasSnapshot() { return true; }

	/**
	 * @brief return the whole stream as a list
	 * @return list of messages
	 */
	QList<MessagePtr> toList() const { return _messages; }

private:
	QList<MessagePtr> _messages;
	int _offset;
};

}

#endif

