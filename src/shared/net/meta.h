/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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
#ifndef DP_NET_META_TRANSPARENT_H
#define DP_NET_META_TRANSPARENT_H

#include "message.h"

#include <QList>
#include <QString>

namespace protocol {

/**
 * @brief Inform the client of a new user
 *
 * This message is sent only be the server. It associates a username
 * with a context ID.
 *
 * The identityHash is not yet used, but is present for forward compatibility.
 * In the future, it will contain a hash made with some secret info (e.g. tripcode or salted IP address)
 * to allow guest users to prove their identity to other users.
 */
class UserJoin : public Message {
public:
	static const uint8_t FLAG_AUTH = 0x01; // authenticated user (not a guest)
	static const uint8_t FLAG_MOD = 0x02;  // user is a moderator

	UserJoin(uint8_t ctx, uint8_t flags, const QByteArray &name, const QByteArray &hash) : Message(MSG_USER_JOIN, ctx), m_name(name), m_hash(hash), m_flags(flags) { Q_ASSERT(name.length()>0 && name.length()<256); }
	UserJoin(uint8_t ctx, uint8_t flags, const QString &name, const QByteArray &hash=QByteArray()) : UserJoin(ctx, flags, name.toUtf8(), hash) {}

	static UserJoin *deserialize(uint8_t ctx, const uchar *data, uint len);
	static UserJoin *fromText(uint8_t ctx, const Kwargs &kwargs);

	QString name() const { return QString::fromUtf8(m_name); }

	QByteArray identityHash() const { return m_hash; }

	uint8_t flags() const { return m_flags; }

	bool isModerator() const { return m_flags & FLAG_MOD; }
	bool isAuthenticated() const { return m_flags & FLAG_AUTH; }

	QString messageName() const override { return QStringLiteral("join"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	QByteArray m_name;
	QByteArray m_hash;
	uint8_t m_flags;
};

/**
 * @brief Inform the client of a user leaving
 *
 * This message is sent only by the server. Upon receiving this message,
 * clients will typically remove the user from the user listing. The client
 * is also allowed to release resources associated with this context ID.
 */
class UserLeave : public ZeroLengthMessage<UserLeave> {
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
class SessionOwner : public Message {
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
 * @brief A chat message
 *
 * Chat message sent by the server with the context ID 0 are server messages.
 * (Typically a Command message is used for server announcements, but the Chat message
 * is used for those messages that must be stored in the session history.)
 */
class Chat : public Message {
public:
	// Transparent flags: these affect serverside behavior
	static const uint8_t FLAG_BYPASS = 0x01; // bypass session history and send directly to logged in users

	// Opaque flags: the server doesn't know anything about these
	static const uint8_t FLAG_SHOUT = 0x01;  // public announcement
	static const uint8_t FLAG_ACTION = 0x02; // this is an "action message" (like /me in IRC)
	static const uint8_t FLAG_PIN = 0x04;    // pin this message

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


}

#endif
