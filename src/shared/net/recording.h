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
#ifndef DP_NET_RECORDING_H
#define DP_NET_RECORDING_H

#include "message.h"

namespace protocol {

/**
 * @brief Event interval record
 *
 * This is used to preserve timing information in session recordings.
 *
 * Note. The maximum interval (using a single message) is about 65 seconds.
 * Typically the intervals we want to store are a few seconds at most, so this should be enough.
 */
class Interval : public Message
{
public:
	Interval(uint16_t milliseconds) : Message(MSG_INTERVAL, 0), _msecs(milliseconds) {}

	static Interval *deserialize(const uchar *data, uint len);

	uint16_t milliseconds() const { return _msecs; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint16_t _msecs;
};

}

#endif
