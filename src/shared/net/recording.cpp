/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2015 Calle Laakkonen

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

Interval *Interval::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=2)
		return 0;
	return new Interval(ctx, qFromBigEndian<quint16>(data));
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

Marker *Marker::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	return new Marker(
		ctx,
		QByteArray((const char*)data, len)
	);
}

int Marker::payloadLength() const
{
	return _text.length();
}

int Marker::serializePayload(uchar *data) const
{
	memcpy(data, _text.constData(), _text.length());
	return _text.length();
}

}
