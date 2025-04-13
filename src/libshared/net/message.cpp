// SPDX-License-Identifier: GPL-3.0-or-later
#include "libshared/net/message.h"
#include "libshared/util/qtcompat.h"
#include <QByteArray>
#include <QJsonDocument>
#include <QString>

namespace net {

Message Message::null()
{
	return Message{nullptr};
}

Message Message::inc(DP_Message *msg)
{
	return Message{DP_message_incref_nullable(msg)};
}

Message Message::noinc(DP_Message *msg)
{
	return Message{msg};
}

Message Message::deserialize(
	const unsigned char *buf, size_t bufsize, bool decodeOpaque)
{
	return Message::noinc(DP_message_deserialize(buf, bufsize, decodeOpaque));
}

Message Message::deserializeWs(
	const unsigned char *buf, size_t bufsize, bool decodeOpaque)
{
	return Message::noinc(
		DP_message_deserialize_length(buf, bufsize, bufsize - 2, decodeOpaque));
}


DP_Message **Message::asRawMessages(const net::Message *msgs)
{
	// We want to do a moderately evil reinterpret cast of a drawdance::Message
	// to its underlying pointer. Let's make sure that it's a valid thing to do.
	// Make sure it's a standard layout class, because only for those it's legal
	// to cast them to their first member.
	static_assert(
		std::is_standard_layout<net::Message>::value,
		"drawdance::Message is standard layout for reinterpretation to "
		"DP_Message");
	// And then ensure that there's only the pointer member.
	static_assert(
		sizeof(net::Message) == sizeof(DP_Message *),
		"drawdance::Message has the same size as a DP_Message pointer");
	// Alright, that means this cast, despite looking terrifying, is legal. The
	// const can be cast away safely too because the underlying pointer isn't.
	return reinterpret_cast<DP_Message **>(const_cast<net::Message *>(msgs));
}


Message::Message()
	: Message(nullptr)
{
}

Message::Message(const Message &other)
	: Message{DP_message_incref_nullable(other.m_data)}
{
}

Message::Message(Message &&other)
	: Message{other.m_data}
{
	other.m_data = nullptr;
}

Message &Message::operator=(const Message &other)
{
	DP_message_decref_nullable(m_data);
	m_data = DP_message_incref_nullable(other.m_data);
	return *this;
}

Message &Message::operator=(Message &&other)
{
	DP_message_decref_nullable(m_data);
	m_data = other.m_data;
	other.m_data = nullptr;
	return *this;
}

Message::~Message()
{
	DP_message_decref_nullable(m_data);
}

DP_Message *Message::get() const
{
	return m_data;
}

bool Message::isNull() const
{
	return !m_data;
}

DP_MessageType Message::type() const
{
	return DP_message_type(m_data);
}

QString Message::typeName() const
{
	return QString::fromUtf8(DP_message_type_name(type()));
}

bool Message::isControl() const
{
	return DP_message_type_control(type());
}

bool Message::isServerMeta() const
{
	return DP_message_type_server_meta(type());
}

bool Message::isInCommandRange() const
{
	return type() >= 128;
}

bool Message::isAllowedInResetImage() const
{
	DP_MessageType t = type();
	return int(t) >= 64 || t == DP_MSG_CHAT;
}

Message Message::asEmergencyMessage() const
{
	switch(type()) {
	case DP_MSG_JOIN: {
		DP_MsgJoin *mj = toJoin();
		size_t nameLen;
		const char *name = DP_msg_join_name(mj, &nameLen);
		return makeJoinMessage(
			contextId(), DP_msg_join_flags(mj),
			QString::fromUtf8(name, compat::sizetype(nameLen)), QByteArray());
	}
	case DP_MSG_LEAVE:
	case DP_MSG_SESSION_OWNER:
		return *this;
	default:
		return null();
	}
}

unsigned int Message::contextId() const
{
	return DP_message_context_id(m_data);
}

void Message::setContextId(unsigned int contextId)
{
	DP_message_context_id_set(m_data, contextId);
}

size_t Message::length() const
{
	return DP_message_length(m_data);
}

size_t Message::wsLength() const
{
	return DP_message_ws_length(m_data);
}

bool Message::equals(const Message &other) const
{
	if(m_data == other.m_data) {
		return true;
	} else if(m_data && other.m_data) {
		return DP_message_equals(m_data, other.m_data);
	} else {
		return false;
	}
}


DP_MsgJoin *Message::toJoin() const
{
	Q_ASSERT(type() == DP_MSG_JOIN);
	return static_cast<DP_MsgJoin *>(DP_message_internal(m_data));
}

DP_MsgChat *Message::toChat() const
{
	Q_ASSERT(type() == DP_MSG_CHAT);
	return static_cast<DP_MsgChat *>(DP_message_internal(m_data));
}

DP_MsgData *Message::toData() const
{
	Q_ASSERT(type() == DP_MSG_DATA);
	return static_cast<DP_MsgData *>(DP_message_internal(m_data));
}

DP_MsgDefaultLayer *Message::toDefaultLayer() const
{
	Q_ASSERT(type() == DP_MSG_DEFAULT_LAYER);
	return static_cast<DP_MsgDefaultLayer *>(DP_message_internal(m_data));
}

DP_MsgDrawDabsClassic *Message::toDrawDabsClassic() const
{
	Q_ASSERT(type() == DP_MSG_DRAW_DABS_CLASSIC);
	return static_cast<DP_MsgDrawDabsClassic *>(DP_message_internal(m_data));
}

DP_MsgDrawDabsPixel *Message::toDrawDabsPixel() const
{
	Q_ASSERT(
		type() == DP_MSG_DRAW_DABS_PIXEL ||
		type() == DP_MSG_DRAW_DABS_PIXEL_SQUARE);
	return static_cast<DP_MsgDrawDabsPixel *>(DP_message_internal(m_data));
}

DP_MsgFeatureAccessLevels *Message::toFeatureAccessLevels() const
{
	Q_ASSERT(type() == DP_MSG_FEATURE_ACCESS_LEVELS);
	return static_cast<DP_MsgFeatureAccessLevels *>(
		DP_message_internal(m_data));
}

DP_MsgFillRect *Message::toFillRect() const
{
	Q_ASSERT(type() == DP_MSG_FILL_RECT);
	return static_cast<DP_MsgFillRect *>(DP_message_internal(m_data));
}

DP_MsgLayerAttributes *Message::toLayerAttributes() const
{
	Q_ASSERT(type() == DP_MSG_LAYER_ATTRIBUTES);
	return static_cast<DP_MsgLayerAttributes *>(DP_message_internal(m_data));
}

DP_MsgLayerTreeCreate *Message::toLayerTreeCreate() const
{
	Q_ASSERT(type() == DP_MSG_LAYER_TREE_CREATE);
	return static_cast<DP_MsgLayerTreeCreate *>(DP_message_internal(m_data));
}

DP_MsgPrivateChat *Message::toPrivateChat() const
{
	Q_ASSERT(type() == DP_MSG_PRIVATE_CHAT);
	return static_cast<DP_MsgPrivateChat *>(DP_message_internal(m_data));
}

DP_MsgPutImage *Message::toPutImage() const
{
	Q_ASSERT(type() == DP_MSG_PUT_IMAGE);
	return static_cast<DP_MsgPutImage *>(DP_message_internal(m_data));
}

DP_MsgResetStream *Message::toResetStream() const
{
	Q_ASSERT(type() == DP_MSG_RESET_STREAM);
	return static_cast<DP_MsgResetStream *>(DP_message_internal(m_data));
}

DP_MsgServerCommand *Message::toServerCommand() const
{
	Q_ASSERT(type() == DP_MSG_SERVER_COMMAND);
	return static_cast<DP_MsgServerCommand *>(DP_message_internal(m_data));
}

DP_MsgSessionOwner *Message::toSessionOwner() const
{
	Q_ASSERT(type() == DP_MSG_SESSION_OWNER);
	return static_cast<DP_MsgSessionOwner *>(DP_message_internal(m_data));
}

DP_MsgTrustedUsers *Message::toTrustedUsers() const
{
	Q_ASSERT(type() == DP_MSG_TRUSTED_USERS);
	return static_cast<DP_MsgTrustedUsers *>(DP_message_internal(m_data));
}

DP_MsgUserAcl *Message::toUserAcl() const
{
	Q_ASSERT(type() == DP_MSG_USER_ACL);
	return static_cast<DP_MsgUserAcl *>(DP_message_internal(m_data));
}


bool Message::serialize(QByteArray &buffer) const
{
	return DP_message_serialize(m_data, true, getDeserializeBuffer, &buffer) !=
		   0;
}

bool Message::serializeWs(QByteArray &buffer) const
{
	return DP_message_serialize(m_data, false, getDeserializeBuffer, &buffer) !=
		   0;
}

bool Message::shouldSmoothe() const
{
	switch(type()) {
	case DP_MSG_DRAW_DABS_CLASSIC:
	case DP_MSG_DRAW_DABS_PIXEL:
	case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
	case DP_MSG_DRAW_DABS_MYPAINT:
	case DP_MSG_DRAW_DABS_MYPAINT_BLEND:
	case DP_MSG_MOVE_POINTER:
		return true;
	default:
		return false;
	}
}

void Message::setUchars(size_t size, unsigned char *out, void *user)
{
	if(size > 0) {
		memcpy(out, user, size);
	}
}

void Message::setUint8s(int count, uint8_t *out, void *user)
{
	if(count > 0) {
		memcpy(out, user, sizeof(uint8_t) * count);
	}
}

void Message::setUint16s(int count, uint16_t *out, void *user)
{
	if(count > 0) {
		memcpy(out, user, sizeof(uint16_t) * count);
	}
}

void Message::setUint24s(int count, uint32_t *out, void *user)
{
	if(count > 0) {
		memcpy(out, user, sizeof(uint32_t) * count);
	}
}

void Message::setInt32s(int count, int32_t *out, void *user)
{
	if(count > 0) {
		memcpy(out, user, sizeof(int32_t) * count);
	}
}

Message::Message(DP_Message *msg)
	: m_data{msg}
{
}

unsigned char *Message::getDeserializeBuffer(void *user, size_t size)
{
	QByteArray *buffer = static_cast<QByteArray *>(user);
	buffer->resize(compat::castSize(size));
	return reinterpret_cast<unsigned char *>(buffer->data());
}


Message makeChatMessage(
	uint8_t contextId, uint8_t tflags, uint8_t oflags, const QString &message)
{
	QByteArray bytes = message.toUtf8();
	return Message::noinc(DP_msg_chat_new(
		contextId, tflags, oflags, bytes.constData(), bytes.length()));
}

Message
makeDisconnectMessage(uint8_t contextId, uint8_t reason, const QString &message)
{
	QByteArray bytes = message.toUtf8();
	return Message::noinc(DP_msg_disconnect_new(
		contextId, reason, bytes.constData(), bytes.length()));
}

Message makeJoinMessage(
	uint8_t contextId, uint8_t flags, const QString &name,
	const QByteArray &avatar)
{
	QByteArray nameBytes = name.toUtf8();
	return Message::noinc(DP_msg_join_new(
		contextId, flags, nameBytes.constData(), nameBytes.size(),
		Message::setUchars, avatar.size(),
		const_cast<char *>(avatar.constData())));
}

Message makeKeepAliveMessage(uint8_t contextId)
{
	return Message::noinc(DP_msg_keep_alive_new(contextId));
}

Message makeLeaveMessage(uint8_t contextId)
{
	return Message::noinc(DP_msg_leave_new(contextId));
}

Message makePingMessage(uint8_t contextId, bool isPong)
{
	return Message::noinc(DP_msg_ping_new(contextId, isPong));
}

Message makePrivateChatMessage(
	uint8_t contextId, uint8_t target, uint8_t oflags, const QString &message)
{
	QByteArray bytes = message.toUtf8();
	return Message::noinc(DP_msg_private_chat_new(
		contextId, target, oflags, bytes.constData(), bytes.length()));
}

Message makeServerCommandMessage(uint8_t contextId, const QJsonDocument &msg)
{
	QByteArray msgBytes = msg.toJson(QJsonDocument::Compact);
	if(msgBytes.length() <=
	   DP_MESSAGE_MAX_PAYLOAD_LENGTH - DP_MSG_SERVER_COMMAND_STATIC_LENGTH) {
		return Message::noinc(DP_msg_server_command_new(
			contextId, msgBytes.constData(), msgBytes.length()));
	} else {
		qWarning(
			"ServerCommand too long (%lld bytes)",
			compat::cast<long long>(msgBytes.length()));
		return Message::null();
	}
}

Message
makeSessionOwnerMessage(uint8_t contextId, const QVector<uint8_t> &users)
{
	return Message::noinc(DP_msg_session_owner_new(
		contextId, Message::setUint8s, users.count(),
		const_cast<uint8_t *>(users.constData())));
}

Message makeSoftResetMessage(uint8_t contextId)
{
	return Message::noinc(DP_msg_soft_reset_new(contextId));
}

Message
makeTrustedUsersMessage(uint8_t contextId, const QVector<uint8_t> &users)
{
	return Message::noinc(DP_msg_trusted_users_new(
		contextId, Message::setUint8s, users.count(),
		const_cast<uint8_t *>(users.constData())));
}

}
