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
    enum Mode {REQUEST, ACK, SNAPSHOT, END};

    SnapshotMode(Mode mode) : Message(MSG_SNAPSHOT), _mode(mode) {}

	static SnapshotMode *deserialize(const uchar *data, int len);

    Mode mode() const { return _mode; }

protected:
    int payloadLength() const;
    int serializePayload(uchar *data) const;

private:
    Mode _mode;
};

}

#endif
