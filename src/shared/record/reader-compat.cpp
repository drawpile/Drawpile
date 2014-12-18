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

#include "reader-compat.h"

#include "../net/message.h"
#include "../net/pen.h"

#include <QtEndian>

namespace recording {
namespace compat {

using namespace protocol;

ToolChange *ToolChangeV11(const uchar *data, uint len)
{
	if(len != 19)
		return 0;

	return new ToolChange(
		*(data+0),
		*(data+1),
		*(data+2),
		*(data+3),
		*(data+4),
		qFromBigEndian<quint32>(data+5),
		qFromBigEndian<quint32>(data+9),
		*(data+13),
		*(data+14),
		qMax(1, *(data+15)*2), // brush size used to mean radius instead of diameter
		qMax(1, *(data+16)*2),
		*(data+17),
		*(data+18),
		0, // smudge parameters were added in V12
		0
	);
}


Message *deserializeV11(const uchar *data, int length)
{
	if(length<Message::HEADER_LEN)
		return nullptr;

	const quint16 len = qFromBigEndian<quint16>(data);

	if(length < len+Message::HEADER_LEN)
		return nullptr;

	const MessageType type = MessageType(data[2]);

	// ToolChange is the only (recordable) message type changed between V11 and V12
	if(type == MSG_TOOLCHANGE) {
		return ToolChangeV11(data + Message::HEADER_LEN, len);
	} else {
		return Message::deserialize(data, length);
	}
}

}
}

