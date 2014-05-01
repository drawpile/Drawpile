/*
   DrawPile - a collaborative drawing program.

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
#include <QtEndian>

#include "recording.h"

namespace protocol {

Interval *Interval::deserialize(const uchar *data, uint len)
{
	if(len!=2)
		return 0;
	return new Interval(qFromBigEndian<quint16>(data));
}

int Interval::payloadLength() const
{
	return 2;
}

int Interval::serializePayload(uchar *data) const
{
	qToBigEndian(_msecs, data);
	return 2;
}

Marker *Marker::deserialize(const uchar *data, uint len)
{
	if(len<1)
		return 0;

	return new Marker(
		*(data+0),
		QByteArray((const char*)data+1, len-1)
	);
}

int Marker::payloadLength() const
{
	return 1 + _text.length();
}

int Marker::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	memcpy(ptr, _text.constData(), _text.length());
	ptr += _text.length();
	return ptr - data;
}

}
