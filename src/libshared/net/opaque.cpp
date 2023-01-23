/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2019 Calle Laakkonen

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

#include "libshared/net/opaque.h"

#include "libshared/net/annotation.h"
#include "libshared/net/image.h"
#include "libshared/net/layer.h"
#include "libshared/net/meta2.h"
#include "libshared/net/brushes.h"
#include "libshared/net/undo.h"
#include "libshared/net/recording.h"

#include <cstring>

namespace protocol {

OpaqueMessage::OpaqueMessage(MessageType type, uint8_t ctx, const uchar *payload, int payloadLen)
	: Message(type, ctx), m_length(payloadLen)
{
	Q_ASSERT(type >= 64);
	if(payloadLen>0) {
		m_payload = new uchar[payloadLen];
		memcpy(m_payload, payload, payloadLen);
	} else {
		m_payload = nullptr;
	}
}

OpaqueMessage::~OpaqueMessage()
{
	delete []m_payload;
}

NullableMessageRef OpaqueMessage::decode(MessageType type, uint8_t ctx, const uchar *data, uint len)
{
	Q_ASSERT(type>=64);

	Message *msg = nullptr;

	switch(type) {
	case MSG_INTERVAL: msg = Interval::deserialize(ctx, data, len); break;
	case MSG_LASERTRAIL: msg = LaserTrail::deserialize(ctx, data, len); break;
	case MSG_MOVEPOINTER: msg = MovePointer::deserialize(ctx, data, len); break;
	case MSG_MARKER: msg = Marker::deserialize(ctx, data, len); break;
	case MSG_USER_ACL: msg = UserACL::deserialize(ctx, data, len); break;
	case MSG_LAYER_ACL: msg = LayerACL::deserialize(ctx, data, len); break;
	case MSG_FEATURE_LEVELS: msg = FeatureAccessLevels::deserialize(ctx, data, len); break;
	case MSG_LAYER_DEFAULT: msg = DefaultLayer::deserialize(ctx, data, len); break;
	case MSG_FILTERED: return Filtered::deserialize(ctx, data, len);

	case MSG_CANVAS_RESIZE: msg = CanvasResize::deserialize(ctx, data, len); break;
	case MSG_LAYER_CREATE: msg = LayerCreate::deserialize(ctx, data, len); break;
	case MSG_LAYER_ATTR: msg = LayerAttributes::deserialize(ctx, data, len); break;
	case MSG_LAYER_RETITLE: msg = LayerRetitle::deserialize(ctx, data, len); break;
	case MSG_LAYER_ORDER: msg = LayerOrder::deserialize(ctx, data, len); break;
	case MSG_LAYER_DELETE: msg = LayerDelete::deserialize(ctx, data, len); break;
	case MSG_LAYER_VISIBILITY: msg = LayerVisibility::deserialize(ctx, data, len); break;
	case MSG_PUTIMAGE: msg = PutImage::deserialize(ctx, data, len); break;
	case MSG_PEN_UP: msg = PenUp::deserialize(ctx, data, len); break;
	case MSG_ANNOTATION_CREATE: msg = AnnotationCreate::deserialize(ctx, data, len); break;
	case MSG_ANNOTATION_RESHAPE: msg = AnnotationReshape::deserialize(ctx, data, len); break;
	case MSG_ANNOTATION_EDIT: msg = AnnotationEdit::deserialize(ctx, data, len); break;
	case MSG_ANNOTATION_DELETE: msg = AnnotationDelete::deserialize(ctx, data, len); break;
	case MSG_UNDOPOINT: msg = UndoPoint::deserialize(ctx, data, len); break;
	case MSG_UNDO: msg = Undo::deserialize(ctx, data, len); break;
	case MSG_FILLRECT: msg = FillRect::deserialize(ctx, data, len); break;
	case MSG_REGION_MOVE: msg = MoveRegion::deserialize(ctx, data, len); break;
	case MSG_PUTTILE: msg = PutTile::deserialize(ctx, data, len); break;
	case MSG_CANVAS_BACKGROUND: msg = CanvasBackground::deserialize(ctx, data, len); break;
	case MSG_DRAWDABS_CLASSIC: msg = DrawDabsClassic::deserialize(ctx, data, len); break;
	case MSG_DRAWDABS_PIXEL: msg = DrawDabsPixel::deserialize(DabShape::Round, ctx, data, len); break;
	case MSG_DRAWDABS_PIXEL_SQUARE: msg = DrawDabsPixel::deserialize(DabShape::Square, ctx, data, len); break;
	default: qWarning("Unhandled opaque message type: %d", type);
	}

	return NullableMessageRef(msg);
}

NullableMessageRef OpaqueMessage::decode() const
{
	return decode(type(), contextId(), m_payload, m_length);
}

int OpaqueMessage::payloadLength() const
{
	return m_length;
}

int OpaqueMessage::serializePayload(uchar *data) const
{
	memcpy(data, m_payload, m_length);
	return m_length;
}

bool OpaqueMessage::payloadEquals(const Message &m) const
{
	const OpaqueMessage &om = static_cast<const OpaqueMessage&>(m);
	if(m_length != om.m_length)
		return false;

	return memcmp(m_payload, om.m_payload, m_length) == 0;
}

}

