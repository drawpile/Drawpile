/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2017 Calle Laakkonen

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
	qToBigEndian(m_msecs, data);
	return 2;
}

Kwargs Interval::kwargs() const
{
	Kwargs kw;
	kw["msecs"] = QString::number(m_msecs);
	return kw;
}

Interval *Interval::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new Interval(
		ctx,
		kwargs["msecs"].toInt()
		);
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
	return m_text.length();
}

int Marker::serializePayload(uchar *data) const
{
	memcpy(data, m_text.constData(), m_text.length());
	return m_text.length();
}

Kwargs Marker::kwargs() const
{
	Kwargs kw;
	kw["text"] = text();
	return kw;
}

Marker *Marker::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new Marker(
		ctx,
		kwargs["text"]
		);
}

}
