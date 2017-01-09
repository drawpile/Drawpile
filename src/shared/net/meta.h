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

	QString name() const { return QString::fromUtf8(m_name); }

	QByteArray identityHash() const { return m_hash; }

	uint8_t flags() const { return m_flags; }

	bool isModerator() const { return m_flags & FLAG_MOD; }
	bool isAuthenticated() const { return m_flags & FLAG_AUTH; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

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

	bool isOpCommand() const { return true; }

	QList<uint8_t> ids() const { return m_ids; }
	void setIds(const QList<uint8_t> ids) { m_ids = ids; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	QList<uint8_t> m_ids;
};

}

#endif
