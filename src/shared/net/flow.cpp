/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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

#include "flow.h"

#include <cstring>
#include <QtEndian>

namespace protocol {

Disconnect *Disconnect::deserialize(const uchar *data, uint len)
{
	if(len<1)
		return 0;
	return new Disconnect(Reason(*data), QByteArray((const char*)data+1, len-1));
}

int Disconnect::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = _reason;
	memcpy(ptr, _message.constData(), _message.length());
	ptr += _message.length();
	return ptr - data;
}

int Disconnect::payloadLength() const
{
	return 1 + _message.length();
}

StreamPos *StreamPos::deserialize(const uchar *data, uint len)
{
	if(len!=4)
		return 0;
	return new StreamPos(qFromBigEndian<quint32>(data));
}

int StreamPos::serializePayload(uchar *data) const
{
	qToBigEndian(_bytes, data);
	return 4;
}

int StreamPos::payloadLength() const
{
	return 4;
}

}

