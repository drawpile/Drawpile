/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include "opaque.h"

#include "annotation.h"
#include "image.h"
#include "layer.h"
#include "meta2.h"
#include "pen.h"
#include "undo.h"
#include "recording.h"

#include <cstring>

namespace protocol {

OpaqueMessage *OpaqueMessage::deserialize(MessageType type, uint8_t ctx, const uchar *data, uint len)
{
	return new OpaqueMessage(type, ctx, QByteArray(reinterpret_cast<const char*>(data), len));
}

Message *OpaqueMessage::decode(MessageType type, uint8_t ctx, const uchar *data, uint len)
{
	Q_ASSERT(type>=64);

	switch(type) {
	case MSG_CHAT: return Chat::deserialize(ctx, data, len);
	case MSG_INTERVAL: return Interval::deserialize(ctx, data, len);
	case MSG_MOVEPOINTER: return MovePointer::deserialize(ctx, data, len);
	case MSG_MARKER: return Marker::deserialize(ctx, data, len);
	case MSG_USER_ACL: return UserACL::deserialize(ctx, data, len);
	case MSG_LAYER_ACL: return LayerACL::deserialize(ctx, data, len);
	case MSG_SESSION_ACL: return SessionACL::deserialize(ctx, data, len);

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
	default:
		qWarning("Unhandled opaque message type: %d", type);
		return nullptr;
	}
}

Message *OpaqueMessage::decode() const
{
	return decode(type(), contextId(), reinterpret_cast<const uchar*>(m_payload.constData()), m_payload.length());
}

int OpaqueMessage::payloadLength() const
{
	return m_payload.length();
}

int OpaqueMessage::serializePayload(uchar *data) const
{
	memcpy(data, m_payload.constData(), m_payload.length());
	return m_payload.length();
}

bool OpaqueMessage::payloadEquals(const Message &m) const
{
	return payload() == static_cast<const OpaqueMessage&>(m).payload();
}

}
