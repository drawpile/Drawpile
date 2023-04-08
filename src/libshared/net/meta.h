// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_NET_META_TRANSPARENT_H
#define DP_NET_META_TRANSPARENT_H

#include "libshared/net/message.h"

#include <QList>
#include <QString>

namespace protocol {

/**
 * @brief Inform the client of a new user
 *
 * This message is sent only be the server. It associates a username
 * with a context ID.
 *
 */
class UserJoin final : public Message {
public:
	static const uint8_t FLAG_AUTH = 0x01; // authenticated user (not a guest)
	static const uint8_t FLAG_MOD = 0x02;  // user is a moderator
	static const uint8_t FLAG_BOT = 0x04;  // user is a bot

	UserJoin(uint8_t ctx, uint8_t flags, const QByteArray &name, const QByteArray &avatar) : Message(MSG_USER_JOIN, ctx), m_name(name), m_avatar(avatar), m_flags(flags) { Q_ASSERT(name.length()>0 && name.length()<256); }
	UserJoin(uint8_t ctx, uint8_t flags, const QString &name, const QByteArray &avatar=QByteArray()) : UserJoin(ctx, flags, name.toUtf8(), avatar) {}

	static UserJoin *deserialize(uint8_t ctx, const uchar *data, uint len);
	static UserJoin *fromText(uint8_t ctx, const Kwargs &kwargs);

	QString name() const { return QString::fromUtf8(m_name); }

	QByteArray avatar() const { return m_avatar; }

	uint8_t flags() const { return m_flags; }

	bool isModerator() const { return m_flags & FLAG_MOD; }
	bool isAuthenticated() const { return m_flags & FLAG_AUTH; }
	bool isBot() const { return m_flags & FLAG_BOT; }

	QString messageName() const override { return QStringLiteral("join"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	QByteArray m_name;
	QByteArray m_avatar;
	uint8_t m_flags;
};

/**
 * @brief Inform the client of a user leaving
 *
 * This message is sent only by the server. Upon receiving this message,
 * clients will typically remove the user from the user listing. The client
 * is also allowed to release resources associated with this context ID.
 */
class UserLeave final : public ZeroLengthMessage<UserLeave> {
public:
	explicit UserLeave(uint8_t ctx) : ZeroLengthMessage(MSG_USER_LEAVE, ctx) {}

	QString messageName() const override { return "leave"; }
};

/**
 * @brief Session ownership change
 *
 * This message sets the users who have operator status. It can be
 * sent by users who are already operators or by the server (ctx=0).
 *
 * The list of operators implicitly contains the user who sends the
 * message, thus users cannot deop themselves.
 *
 * The server sanitizes the ID list so, when distributed to other users,
 * it does not contain any duplicates or non-existing users.
 */
class SessionOwner final : public Message {
public:
	SessionOwner(uint8_t ctx, QList<uint8_t> ids) : Message(MSG_SESSION_OWNER, ctx), m_ids(ids) { }

	static SessionOwner *deserialize(uint8_t ctx, const uchar *data, int buflen);
	static SessionOwner *fromText(uint8_t ctx, const Kwargs &kwargs);

	QList<uint8_t> ids() const { return m_ids; }
	void setIds(const QList<uint8_t> ids) { m_ids = ids; }

	QString messageName() const override { return "owner"; }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	QList<uint8_t> m_ids;
};

/**
 * @brief List of trusted users
 *
 * This message sets the list of user who have been tagged as trusted,
 * but who are not operators. The meaning of "trusted" is a mostly
 * clientside concept, but the session can be configured to allow trusted
 * users access to some operator commands. (Deputies)
 *
 * This command can be sent by operators or by the server (ctx=0).
 *
 * The server sanitizes the ID list so, when distributed to other users,
 * it does not contain any duplicates or non-existing users.
 */
class TrustedUsers final : public Message {
public:
	TrustedUsers(uint8_t ctx, QList<uint8_t> ids) : Message(MSG_TRUSTED_USERS, ctx), m_ids(ids) { }

	static TrustedUsers *deserialize(uint8_t ctx, const uchar *data, int buflen);
	static TrustedUsers *fromText(uint8_t ctx, const Kwargs &kwargs);

	QList<uint8_t> ids() const { return m_ids; }
	void setIds(const QList<uint8_t> ids) { m_ids = ids; }

	QString messageName() const override { return "trusted"; }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	QList<uint8_t> m_ids;
};

/**
 * @brief A chat message
 *
 * Chat message sent by the server with the context ID 0 are server messages.
 * (Typically a Command message is used for server announcements, but the Chat message
 * is used for those messages that must be stored in the session history.)
 */
class Chat final : public Message {
public:
	// Transparent flags: these affect serverside behavior
	static const uint8_t FLAG_BYPASS = 0x01; // bypass session history and send directly to logged in users

	// Opaque flags: the server doesn't know anything about these
	static const uint8_t FLAG_SHOUT = 0x01;  // public announcement
	static const uint8_t FLAG_ACTION = 0x02; // this is an "action message" (like /me in IRC)
	static const uint8_t FLAG_PIN = 0x04;    // pin this message
	static const uint8_t FLAG_ALERT = 0x08;  // high priority alert (can be send by operators only)

	Chat(uint8_t ctx, uint8_t tflags, uint8_t oflags, const QByteArray &msg) : Message(MSG_CHAT, ctx), m_tflags(tflags), m_oflags(oflags), m_msg(msg) {}
	Chat(uint8_t ctx, uint8_t tflags, uint8_t oflags, const QString &msg) : Chat(ctx, tflags, oflags, msg.toUtf8()) {}

	//! Construct a regular chat message
	static MessagePtr regular(uint8_t ctx, const QString &message, bool bypass) { return MessagePtr(new Chat(ctx, bypass ? FLAG_BYPASS : 0, 0, message.toUtf8())); }

	//! Construct a public announcement message
	static MessagePtr announce(uint8_t ctx, const QString &message) { return MessagePtr(new Chat(ctx, 0, FLAG_SHOUT, message.toUtf8())); }

	//! Construct an action type message
	static MessagePtr action(uint8_t ctx, const QString &message, bool bypass) { return MessagePtr(new Chat(ctx, bypass ? FLAG_BYPASS : 0, FLAG_ACTION, message.toUtf8())); }

	//! Construct a pinned message
	static MessagePtr pin(uint8_t ctx, const QString &message) { return MessagePtr(new Chat(ctx, 0, FLAG_SHOUT|FLAG_PIN, message.toUtf8())); }

	static Chat *deserialize(uint8_t ctx, const uchar *data, uint len);
	static Chat *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint8_t transparentFlags() const { return m_tflags; }
	uint8_t opaqueFlags() const { return m_oflags; }

	QString message() const { return QString::fromUtf8(m_msg); }

	/**
	 * @brief Is this a history bypass message?
	 *
	 * If this flag is set, the message is sent directly to other users and not included
	 * in the session history.
	 */
	bool isBypass() const { return m_tflags & FLAG_BYPASS; }

	/**
	 * @brief Is this a shout?
	 *
	 * Shout messages are highlighted so they stand out. Typically used
	 * without the BYPASS flag.
	 *
	 * Clientside only.
	 */
	bool isShout() const { return m_oflags & FLAG_SHOUT; }

	/**
	 * @brief Is this an action message?
	 *
	 * Clientside only.
	 */
	bool isAction() const { return m_oflags & FLAG_ACTION; }

	/**
	 * @brief Is this a pinned chat message?
	 *
	 * Clientside only. Requires OP privileges.
	 */
	bool isPin() const { return m_oflags & FLAG_PIN; }

	QString messageName() const override { return QStringLiteral("chat"); }

protected:
    int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint8_t m_tflags;
	uint8_t m_oflags;
	QByteArray m_msg;
};

/**
 * @brief A private chat message
 *
 * Note. This message type was added in protocol 4.21.2 (v. 2.1.0). For backward compatiblity,
 * the server will not send any private messages from itself; it will only relay them from
 * other users.
 *
 * Private messages always bypass the session history.
 */
class PrivateChat final : public Message {
public:
	// Opaque flags: the server doesn't know anything about these
	static const uint8_t FLAG_ACTION = 0x02; // this is an "action message" (like /me in IRC)

	PrivateChat(uint8_t ctx, uint8_t target, uint8_t oflags, const QByteArray &msg) : Message(MSG_PRIVATE_CHAT, ctx), m_target(target), m_oflags(oflags), m_msg(msg) {}
	PrivateChat(uint8_t ctx, uint8_t target, uint8_t oflags, const QString &msg) : PrivateChat(ctx, target, oflags, msg.toUtf8()) {}

	//! Construct a regular chat message
	static MessagePtr regular(uint8_t ctx, uint8_t target, const QString &message) { return MessagePtr(new PrivateChat(ctx, target, 0, message.toUtf8())); }

	//! Construct an action type message
	static MessagePtr action(uint8_t ctx, uint8_t target, const QString &message) { return MessagePtr(new PrivateChat(ctx, target, FLAG_ACTION, message.toUtf8())); }

	static PrivateChat *deserialize(uint8_t ctx, const uchar *data, uint len);
	static PrivateChat *fromText(uint8_t ctx, const Kwargs &kwargs);

	//! Recipient ID
	uint8_t target() const { return m_target; }

	uint8_t opaqueFlags() const { return m_oflags; }

	QString message() const { return QString::fromUtf8(m_msg); }

	//! Is this an action message? (client side only)
	bool isAction() const { return m_oflags & FLAG_ACTION; }

	QString messageName() const override { return QStringLiteral("pm"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint8_t m_target;
	uint8_t m_oflags;
	QByteArray m_msg;
};

/**
 * @brief Soft reset point marker
 *
 * This message marks the point in the session history where soft reset occurs.
 * Soft resetting is not actually implemented yet; this is here for forward compatiblity.
 *
 * All users should truncate their own session history when receiving this message,
 * since undos cannot cross the reset boundary.
 *
 * The current client implementation handles the history truncation part. This is
 * enough to be compatible with future clients capable of initiating soft reset.
 */
class SoftResetPoint final : public ZeroLengthMessage<SoftResetPoint> {
public:
	explicit SoftResetPoint(uint8_t ctx) : ZeroLengthMessage(MSG_SOFTRESET, ctx) { }
	QString messageName() const override { return QStringLiteral("softreset"); }
};

}

#endif
