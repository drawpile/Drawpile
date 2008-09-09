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

	char type = Utils::read8(buf);
	quint16 len = Utils::read16(buf);

	// Do some sanity checks
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
			if(len>2048) // Any longer than this is suspicious
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
	// If the message passed the sanity checks, return the length + 3 header len
	return len+3;
}

Packet *Packet::deserialize(const QByteArray& data) {
	QBuffer buf(const_cast<QByteArray*>(&data));
	buf.open(QBuffer::ReadOnly);

	char type = Utils::read8(buf);
	quint16 len = Utils::read16(buf);

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

	buf.putChar(_type);
	Utils::write16(buf, payloadLength());
	serializeBody(buf);

	return buf.buffer();
}

}
