/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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
#include <QObject>
#include <QtEndian>

#include "message.h"
#include "control.h"
#include "meta.h"
#include "opaque.h"

namespace protocol {

int Message::sniffLength(const char *data)
{
	// extract payload length
	quint16 len = qFromBigEndian<quint16>((uchar*)data);

	// return total message length
	return len + HEADER_LEN;
}

int Message::serialize(char *data) const
{
	// Fixed header: payload length + message type + context ID
	qToBigEndian(quint16(payloadLength()), (uchar*)data); data += 2;
	*(data++) = m_type;
	*(data++) = m_contextid;

	// Message payload. (May be 0 length)
	int written = serializePayload((uchar*)data);
	Q_ASSERT(written == payloadLength());
	Q_ASSERT(written <= 0xffff);

	return HEADER_LEN + written;
}

bool Message::equals(const Message &m) const
{
	if(type() != m.type() || contextId() != m.contextId())
		return false;

	return payloadEquals(m);
}

bool Message::payloadEquals(const Message &m) const
{
#if 0
	qDebug("default inefficient Message::payloadEquals called (type=%d).", type());
#endif

	if(payloadLength() != m.payloadLength())
		return false;

	QByteArray b1(payloadLength(), 0);
	QByteArray b2(payloadLength(), 0);

	serializePayload((uchar*)b1.data());
	m.serializePayload((uchar*)b2.data());

	return b1 == b2;
}

Message *Message::deserialize(const uchar *data, int buflen, bool decodeOpaque)
{
	// All valid messages have the fixed length header
	if(buflen<HEADER_LEN)
		return nullptr;

	const quint16 len = qFromBigEndian<quint16>(data);

	if(buflen < len+HEADER_LEN)
		return nullptr;

	const MessageType type = MessageType(data[2]);
	const uint8_t ctx = data[3];

	data += HEADER_LEN;

	switch(type) {
	// Control messages
	case MSG_COMMAND: return Command::deserialize(ctx, data, len);
	case MSG_DISCONNECT: return Disconnect::deserialize(ctx, data, len);
	case MSG_PING: return Ping::deserialize(ctx, data, len);
	case MSG_STREAMPOS: return StreamPos::deserialize(ctx, data, len);
	case MSG_INTERNAL: { qWarning("Tried to deserialize MSG_INTERVAL"); return nullptr; }

	// Transparent meta messages
	case MSG_USER_JOIN: return UserJoin::deserialize(ctx, data, len);
	case MSG_USER_LEAVE: return UserLeave::deserialize(ctx, data, len);
	case MSG_SESSION_OWNER: return SessionOwner::deserialize(ctx, data, len);
	case MSG_CHAT: return Chat::deserialize(ctx, data, len);

	// Opaque messages
	default:
		if(type >= 64) {
			if(decodeOpaque)
				return OpaqueMessage::decode(type, ctx, data, len);
			else
				return new OpaqueMessage(type, ctx, data, len);
		}
	}

	// Unknown message type!
	qWarning("Unhandled message type %d", type);
	return nullptr;
}

}
