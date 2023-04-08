// SPDX-License-Identifier: GPL-3.0-or-later

#include "libshared/net/message.h"
#include "libshared/net/control.h"
#include "libshared/net/meta.h"
#include "libshared/net/opaque.h"
#include "libshared/net/recording.h"

#include <QObject>
#include <QtEndian>
#include <QRegularExpression>

namespace protocol {

int Message::sniffLength(const char *data)
{
	// extract payload length
	quint16 len = qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(data));

	// return total message length
	return len + HEADER_LEN;
}

int Message::serialize(char *data) const
{
	// Fixed header: payload length + message type + context ID
	qToBigEndian(quint16(payloadLength()), reinterpret_cast<uchar*>(data)); data += 2;
	*(data++) = m_type;
	*(data++) = m_contextid;

	// Message payload. (May be 0 length)
	int written = serializePayload(reinterpret_cast<uchar*>(data));
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

	serializePayload(reinterpret_cast<uchar*>(b1.data()));
	m.serializePayload(reinterpret_cast<uchar*>(b2.data()));

	return b1 == b2;
}

NullableMessageRef Message::deserialize(const uchar *data, int buflen, bool decodeOpaque)
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

	Message *msg = nullptr;

	switch(type) {
	// Control messages
	case MSG_COMMAND: msg = Command::deserialize(ctx, data, len); break;
	case MSG_DISCONNECT: msg = Disconnect::deserialize(ctx, data, len); break;
	case MSG_PING: msg = Ping::deserialize(ctx, data, len); break;
	case MSG_INTERNAL:
	   qWarning("Tried to deserialize MSG_INTERVAL");
	   return NullableMessageRef();

	// Transparent meta messages
	case MSG_USER_JOIN: msg = UserJoin::deserialize(ctx, data, len); break;
	case MSG_USER_LEAVE: msg = UserLeave::deserialize(ctx, data, len); break;
	case MSG_SESSION_OWNER: msg = SessionOwner::deserialize(ctx, data, len); break;
	case MSG_CHAT: msg = Chat::deserialize(ctx, data, len); break;
	case MSG_TRUSTED_USERS: msg = TrustedUsers::deserialize(ctx, data, len); break;
	case MSG_SOFTRESET: msg = SoftResetPoint::deserialize(ctx, data, len); break;
	case MSG_PRIVATE_CHAT: msg = PrivateChat::deserialize(ctx, data, len); break;

	// Opaque messages
	default:
		if(type >= 64) {
			if(decodeOpaque)
				return OpaqueMessage::decode(type, ctx, data, len);
			else
				msg = new OpaqueMessage(type, ctx, data, len);
		}
	}

	if(!msg)
		qWarning("Unhandled message type %d", type);
	return NullableMessageRef(msg);
}

QString Message::toString() const
{
	const Kwargs kw = kwargs();
	QString str = QStringLiteral("%1 %2").arg(contextId()).arg(messageName());

	// Add non-multiline keyword args
	const QRegularExpression space("\\s");
	bool hasMultiline = false;
	{
		KwargsIterator i(kw);
		while(i.hasNext()) {
			i.next();
			if(i.value().contains(space)) {
				hasMultiline = true;
			} else {
				str = str + ' ' + i.key() + '=' + i.value();
			}
		}
	}

	// Add multiline keyword args
	if(hasMultiline) {
		str += " {";
		KwargsIterator i(kw);
		i.toFront();
		do {
			i.next();
			if(i.value().contains(space)) {
				QStringList lines = i.value().split('\n');
				for(const QString &line : lines) {
					str += "\n\t";
					str += i.key();
					str += "=";
					str += line;
				}
			}
		} while(i.hasNext());
		str += "\n}";
	}
	return str;
}

MessagePtr Message::asFiltered() const
{
	Q_ASSERT(type() != MSG_FILTERED); // no nested wrappings please
	int len = 1 + payloadLength();
	uchar *payload = new uchar[len];

	payload[0] = type();
	serializePayload(payload+1);

	return MessagePtr(new Filtered(contextId(), payload, qMin(len, 0xffff)));
}

}

