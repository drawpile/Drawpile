// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBSHARED_DRAWDANCE_MESSAGE_H
#define LIBSHARED_DRAWDANCE_MESSAGE_H
extern "C" {
#include <dpmsg/message.h>
}
#include <QMetaType>
#include <QVector>

class QByteArray;
class QJsonDocument;
class QString;
struct DP_Message;

namespace net {

using MessageList = QVector<class Message>;

class Message final {
public:
	static Message null();
	static Message inc(DP_Message *cs);
	static Message noinc(DP_Message *cs);

	static Message
	deserialize(const unsigned char *buf, size_t bufsize, bool decodeOpaque);

	static Message
	deserializeWs(const unsigned char *buf, size_t bufsize, bool decodeOpaque);

	static DP_Message **asRawMessages(const Message *msgs);

	Message();
	Message(const Message &other);
	Message(Message &&other);

	Message &operator=(const Message &other);
	Message &operator=(Message &&other);

	~Message();

	DP_Message *get() const;

	bool isNull() const;

	DP_MessageType type() const;
	QString typeName() const;

	bool isControl() const;
	bool isServerMeta() const;
	bool isInCommandRange() const;
	bool isAllowedInResetImage() const;

	// Returns user joins without the avatar; leaves and session owners as-is.
	// Any other message type results in a null message, since they're not
	// relevant in a space emergency situation.
	Message asEmergencyMessage() const;

	unsigned int contextId() const;
	void setContextId(unsigned int contextId);

	void setIndirectCompatFlag();

	size_t length() const;
	size_t wsLength() const;

	bool equals(const Message &other) const;

	DP_MsgJoin *toJoin() const;
	DP_MsgChat *toChat() const;
	DP_MsgData *toData() const;
	DP_MsgDefaultLayer *toDefaultLayer() const;
	DP_MsgDrawDabsClassic *toDrawDabsClassic() const;
	DP_MsgDrawDabsPixel *toDrawDabsPixel() const;
	DP_MsgFeatureAccessLevels *toFeatureAccessLevels() const;
	DP_MsgFillRect *toFillRect() const;
	DP_MsgLayerAttributes *toLayerAttributes() const;
	DP_MsgLayerTreeCreate *toLayerTreeCreate() const;
	DP_MsgPrivateChat *toPrivateChat() const;
	DP_MsgPutImage *toPutImage() const;
	DP_MsgResetStream *toResetStream() const;
	DP_MsgServerCommand *toServerCommand() const;
	DP_MsgSessionOwner *toSessionOwner() const;
	DP_MsgTrustedUsers *toTrustedUsers() const;
	DP_MsgUserAcl *toUserAcl() const;

	bool serialize(QByteArray &buffer) const;
	bool serializeWs(QByteArray &buffer) const;

	bool shouldSmoothe() const;

	static void setUchars(size_t size, unsigned char *out, void *user);
	static void setUint8s(int count, uint8_t *out, void *user);
	static void setUint16s(int count, uint16_t *out, void *user);

private:
	explicit Message(DP_Message *cs);

	static unsigned char *getDeserializeBuffer(void *user, size_t size);

	DP_Message *m_data;
};

Message makeChatMessage(
	uint8_t contextId, uint8_t tflags, uint8_t oflags, const QString &message);

Message makeDisconnectMessage(
	uint8_t contextId, uint8_t reason, const QString &message);

Message makeJoinMessage(
	uint8_t contextId, uint8_t flags, const QString &name,
	const QByteArray &avatar);

Message makeKeepAliveMessage(uint8_t contextId);

Message makeLeaveMessage(uint8_t contextId);

Message makePingMessage(uint8_t contextId, bool isPong);

Message makePrivateChatMessage(
	uint8_t contextId, uint8_t target, uint8_t oflags, const QString &message);

Message makeServerCommandMessage(uint8_t contextId, const QJsonDocument &msg);

Message
makeSessionOwnerMessage(uint8_t contextId, const QVector<uint8_t> &users);

Message makeSoftResetMessage(uint8_t contextId);

Message
makeTrustedUsersMessage(uint8_t contextId, const QVector<uint8_t> &users);

}

Q_DECLARE_METATYPE(net::Message)

#endif
