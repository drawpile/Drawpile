/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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
#include "layer.h"
#include "login.h"
#include "meta.h"
#include "flow.h"
#include "pen.h"
#include "snapshot.h"
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
	// Fixed header: payload length + message type
	qToBigEndian(quint16(payloadLength()), (uchar*)data); data += 2;
	*(data++) = _type;

	// Message payload. (May be 0 length)
	int written = serializePayload((uchar*)data);
	Q_ASSERT(written == payloadLength());
	Q_ASSERT(written <= 0xffff);

	return HEADER_LEN + written;
}

Message *Message::deserialize(const uchar *data, int buflen)
{
	// All valid messages have the fixed length header
	if(buflen<HEADER_LEN)
		return 0;

	const quint16 len = qFromBigEndian<quint16>(data);

	if(buflen < len+HEADER_LEN)
		return 0;

	const MessageType type = MessageType(data[2]);
	data += HEADER_LEN;

	switch(type) {
	case MSG_LOGIN: return Login::deserialize(data, len);

	case MSG_USER_JOIN: return UserJoin::deserialize(data, len);
	case MSG_USER_ATTR: return UserAttr::deserialize(data, len);
	case MSG_USER_LEAVE: return UserLeave::deserialize(data, len);
	case MSG_CHAT: return Chat::deserialize(data, len);
	case MSG_LAYER_ACL: return LayerACL::deserialize(data, len);
	case MSG_SNAPSHOT: return SnapshotMode::deserialize(data, len);
	case MSG_SNAPSHOTPOINT: return 0; /* this message is for internal use only */
	case MSG_SESSION_TITLE: return SessionTitle::deserialize(data, len);
	case MSG_SESSION_CONFIG: return SessionConf::deserialize(data, len);
	case MSG_STREAMPOS: return StreamPos::deserialize(data, len);
	case MSG_INTERVAL: return Interval::deserialize(data, len);
	case MSG_MOVEPOINTER: return MovePointer::deserialize(data, len);
	case MSG_MARKER: return Marker::deserialize(data, len);
	case MSG_DISCONNECT: return Disconnect::deserialize(data, len);

	case MSG_CANVAS_RESIZE: return CanvasResize::deserialize(data, len);
	case MSG_LAYER_CREATE: return LayerCreate::deserialize(data, len);
	case MSG_LAYER_COPY: return 0; /* reserved for future use */
	case MSG_LAYER_ATTR: return LayerAttributes::deserialize(data, len);
	case MSG_LAYER_RETITLE: return LayerRetitle::deserialize(data, len);
	case MSG_LAYER_ORDER: return LayerOrder::deserialize(data, len);
	case MSG_LAYER_DELETE: return LayerDelete::deserialize(data, len);
	case MSG_PUTIMAGE: return PutImage::deserialize(data, len);
	case MSG_TOOLCHANGE: return ToolChange::deserialize(data, len);
	case MSG_PEN_MOVE: return PenMove::deserialize(data, len);
	case MSG_PEN_UP: return PenUp::deserialize(data, len);
	case MSG_ANNOTATION_CREATE: return AnnotationCreate::deserialize(data, len);
	case MSG_ANNOTATION_RESHAPE: return AnnotationReshape::deserialize(data, len);
	case MSG_ANNOTATION_EDIT: return AnnotationEdit::deserialize(data, len);
	case MSG_ANNOTATION_DELETE: return AnnotationDelete::deserialize(data, len);
	case MSG_UNDOPOINT: return UndoPoint::deserialize(data, len);
	case MSG_UNDO: return Undo::deserialize(data, len);
	case MSG_FILLRECT: return FillRect::deserialize(data, len);
	}
	// Unknown message type!
	return 0;
}

}
