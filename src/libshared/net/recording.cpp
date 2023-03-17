/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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
#include "libshared/net/recording.h"
#include "libshared/net/opaque.h"

#include <QtEndian>
#include <cstring>

namespace protocol {

Interval *Interval::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=2)
		return nullptr;
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
		QByteArray(reinterpret_cast<const char*>(data), len)
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


Filtered::Filtered(uint8_t ctx, uchar *payload, int payloadLen)
	: Message(MSG_FILTERED, ctx), m_payload(payload), m_length(payloadLen)
{ }

Filtered::~Filtered()
{
	delete []m_payload;
}

NullableMessageRef Filtered::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len<1 || len > 0xffff)
		return nullptr;

	uchar *payload = new uchar[len];
	memcpy(payload, data, len);

	return NullableMessageRef(new Filtered(ctx, payload, len));
}

NullableMessageRef Filtered::decodeWrapped() const
{
	// Note: technically non-opaque messages could be wrapped as well,
	// but in practice they never are. Non-opaque messages are filtered
	// by the server, so there is never need to filter them on the client side.
	return OpaqueMessage::decode(wrappedType(), contextId(), m_payload+1, wrappedPayloadLength());
}

int Filtered::serializePayload(uchar *data) const
{
	memcpy(data, m_payload, m_length);
	return m_length;
}

bool Filtered::payloadEquals(const Message &m) const
{
        const Filtered &fm = static_cast<const Filtered&>(m);
        if(m_length != fm.m_length)
                return false;

        return memcmp(m_payload, fm.m_payload, m_length) == 0;
}

}

