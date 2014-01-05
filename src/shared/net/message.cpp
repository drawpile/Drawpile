#include <QObject>
#include <QtEndian>

#include "message.h"

#include "annotation.h"
#include "image.h"
#include "layer.h"
#include "login.h"
#include "meta.h"
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
	return len + 3;
}

int Message::serialize(char *data) const
{
	qToBigEndian(quint16(payloadLength()), (uchar*)data); data += 2;
	*(data++) = _type;

	int written = serializePayload((uchar*)data);
	Q_ASSERT(written == payloadLength());
	Q_ASSERT(written + 3 <= 0xffff);

	return 3 + written;
}

Message *Message::deserialize(const uchar *data)
{
	quint16 len = qFromBigEndian<quint16>(data);

	MessageType type = MessageType(data[2]);
	data += 3;

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

	case MSG_CANVAS_RESIZE: return CanvasResize::deserialize(data, len);
	case MSG_LAYER_CREATE: return LayerCreate::deserialize(data, len);
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
	}
	// Unknown message type!
	return 0;
}

}
