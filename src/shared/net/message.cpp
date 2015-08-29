/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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

#include "annotation.h"
#include "image.h"
#include "control.h"
#include "layer.h"
#include "meta.h"
#include "meta2.h"
#include "flow.h"
#include "pen.h"
#include "undo.h"
#include "recording.h"

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

Message *Message::deserialize(const uchar *data, int buflen)
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
	case MSG_COMMAND: return Command::deserialize(ctx, data, len);
	case MSG_DISCONNECT: return Disconnect::deserialize(ctx, data, len);
	case MSG_PING: return Ping::deserialize(ctx, data, len);
	case MSG_STREAMPOS: return StreamPos::deserialize(ctx, data, len);

	case MSG_USER_JOIN: return UserJoin::deserialize(ctx, data, len);
	case MSG_USER_LEAVE: return UserLeave::deserialize(ctx, data, len);
	case MSG_SESSION_OWNER: return SessionOwner::deserialize(ctx, data, len);

	case MSG_CHAT: return Chat::deserialize(ctx, data, len);
	case MSG_INTERVAL: return Interval::deserialize(ctx, data, len);
	case MSG_MOVEPOINTER: return MovePointer::deserialize(ctx, data, len);
	case MSG_MARKER: return Marker::deserialize(ctx, data, len);
	case MSG_LAYER_ACL: return LayerACL::deserialize(ctx, data, len);

	case MSG_CANVAS_RESIZE: return CanvasResize::deserialize(ctx, data, len);
	case MSG_LAYER_CREATE: return LayerCreate::deserialize(ctx, data, len);
	case MSG_LAYER_ATTR: return LayerAttributes::deserialize(ctx, data, len);
	case MSG_LAYER_RETITLE: return LayerRetitle::deserialize(ctx, data, len);
	case MSG_LAYER_ORDER: return LayerOrder::deserialize(ctx, data, len);
	case MSG_LAYER_DELETE: return LayerDelete::deserialize(ctx, data, len);
	case MSG_PUTIMAGE: return PutImage::deserialize(ctx, data, len);
	case MSG_TOOLCHANGE: return ToolChange::deserialize(ctx, data, len);
	case MSG_PEN_MOVE: return PenMove::deserialize(ctx, data, len);
	case MSG_PEN_UP: return PenUp::deserialize(ctx, data, len);
	case MSG_ANNOTATION_CREATE: return AnnotationCreate::deserialize(ctx, data, len);
	case MSG_ANNOTATION_RESHAPE: return AnnotationReshape::deserialize(ctx, data, len);
	case MSG_ANNOTATION_EDIT: return AnnotationEdit::deserialize(ctx, data, len);
	case MSG_ANNOTATION_DELETE: return AnnotationDelete::deserialize(ctx, data, len);
	case MSG_UNDOPOINT: return UndoPoint::deserialize(ctx, data, len);
	case MSG_UNDO: return Undo::deserialize(ctx, data, len);
	case MSG_FILLRECT: return FillRect::deserialize(ctx, data, len);
	}
	// Unknown message type!
	return nullptr;
}

}
