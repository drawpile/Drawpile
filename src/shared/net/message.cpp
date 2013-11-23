#include <QObject>
#include <QtEndian>

#include "message.h"

#include "annotation.h"
#include "image.h"
#include "layer.h"
#include "login.h"
#include "pen.h"
#include "snapshot.h"

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
	*data = _type; ++data;
	int written = serializePayload((uchar*)data);
	Q_ASSERT(written == payloadLength());
	return 3+written;
}

Message *Message::deserialize(const uchar *data)
{
	quint16 len = qFromBigEndian<quint16>(data);

	MessageType type = MessageType(data[2]);
	data += 3;

	switch(type) {
	case MSG_LOGIN: return Login::deserialize(data, len);
	case MSG_CANVAS_RESIZE: return CanvasResize::deserialize(data, len);
	case MSG_LAYER_CREATE: return LayerCreate::deserialize(data, len);
	case MSG_LAYER_ATTR: return LayerAttributes::deserialize(data, len);
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
	case MSG_UNDO: return 0;
	case MSG_REDO: return 0;
	case MSG_USER_JOIN: return 0;
	case MSG_USER_ATTR: return 0;
	case MSG_USER_LEAVE: return 0;
	case MSG_CHAT: return 0;
	case MSG_LAYER_ACL: return 0;
	case MSG_SNAPSHOT: return SnapshotMode::deserialize(data, len);
	case MSG_STREAMPOS: return 0;
	}
	// Unknown message type!
	return 0;
}

}
