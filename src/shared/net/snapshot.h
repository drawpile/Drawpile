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
#ifndef DP_NET_SNAPSHOT_H
#define DP_NET_SNAPSHOT_H

#include <QList>
#include "message.h"

namespace protocol {

/**
 * @brief The Snapshot mode message is used when transferring a snapshot point.
 *
 * It is used to request the generation of a snapshot as well as a modifier
 * to indicate the next message is part of the snapshot stream.
 */
class SnapshotMode : public Message {
public:
	enum Mode {REQUEST, REQUEST_NEW, ACK, SNAPSHOT, END};

	SnapshotMode(Mode mode) : Message(MSG_SNAPSHOT, 0), _mode(mode) {}

	static SnapshotMode *deserialize(const uchar *data, int len);

    Mode mode() const { return _mode; }

protected:
    int payloadLength() const;
    int serializePayload(uchar *data) const;

private:
    Mode _mode;
};

/**
 * @brief A snapshot point container
 *
 * This message is never actually transmitted over the network. It
 * serves as a container for the snapshot point stream in the actual
 * command stream.
 */
class SnapshotPoint : public Message {
public:
	SnapshotPoint() : Message(MSG_SNAPSHOT, 0), _complete(false) {}

	/**
	 * @brief Get the snapshot point substream
	 * @return list of snapshot commands
	 */
	const QList<MessagePtr> &substream() const { return _substream; }

	/**
	 * @brief Add a message to the snapshot point
	 *
	 * Adding a SnapshotMode::END message marks this snapshot point as complete.
	 * No messages after it are accepted.
	 * @param msg
	 */
	void append(MessagePtr msg);

	/**
	 * @brief Check if this snapshot point is complete
	 *
	 * A client's main stream pointer is not advanced until the
	 * snapshot stream is complete, even if it has consumed all
	 * available snasphot commands, as there are still more on the way.
	 * @return true if substream is complete
	 */
	bool isComplete() const { return _complete; }

protected:
	int payloadLength() const { return 0; }
	int serializePayload(uchar*) const { return 0; }

private:
	QList<MessagePtr> _substream;
	bool _complete;

};
}

#endif
