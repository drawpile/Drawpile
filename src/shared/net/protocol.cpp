/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

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

#include <QBuffer>

#include "message.h"
#include "stroke.h"
#include "toolselect.h"
#include "binary.h"
#include "login.h"
#include "utils.h"

namespace protocol {

unsigned int Packet::sniffLength(const QByteArray& data) {
	QBuffer buf(const_cast<QByteArray*>(&data));
	buf.open(QBuffer::ReadOnly);

	// Read the packet length (incl. the type field)
	quint16 len = utils::read16(buf);
	if(len<1)
		return 0;
	--len;
	char type = utils::read8(buf);

	switch(type) {
		case STROKE:
			if((len-1)%5)
				return 0;
			break;
		case STROKE_END:
			if(len!=1)
				return 0;
			break;
		case TOOL_SELECT:
			if(len!=16)
				return 0;
			break;
		case MESSAGE:
			if(len>2048) // Any longer than this is suspicious (arbitrary)
				return 0;
			break;
		case BINARY_CHUNK:
			// Anything goes really...
			break;
		case LOGIN_ID:
			if(len!=8)
				return 0;
			break;
		default:
			// Unknown packet type
			return 0;
	}
	// If the message passed the sanity checks, return the full lenght of the
	// packet (length and type fields included)
	return len+3;
}

Packet *Packet::deserialize(const QByteArray& data) {
	QBuffer buf(const_cast<QByteArray*>(&data));
	buf.open(QBuffer::ReadOnly);

	quint16 len = utils::read16(buf) - 1;
	char type = utils::read8(buf);

	// Deserialize correct message type
	switch(type) {
		case STROKE: return StrokePoint::deserialize(buf, len);
		case STROKE_END: return StrokeEnd::deserialize(buf, len);
		case TOOL_SELECT: return ToolSelect::deserialize(buf, len);
		case MESSAGE: return Message::deserialize(buf, len);
		case BINARY_CHUNK: return BinaryChunk::deserialize(buf, len);
		case LOGIN_ID: return LoginId::deserialize(buf, len);
		default: qWarning("Unrecognized packet type %d with length %d", type, len); return 0;
	}
}

QByteArray Packet::serialize() const {
	QBuffer buf;
	buf.open(QBuffer::WriteOnly);

	utils::write16(buf, payloadLength() + 1);
	buf.putChar(_type);
	serializeBody(buf);

	return buf.buffer();
}

}
