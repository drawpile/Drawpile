/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#ifndef DP_NET_META_H
#define DP_NET_META_H

#include <QString>

#include "message.h"

namespace protocol {

/**
 * @brief Inform the client of a new user
 *
 * This message is sent only be the server. It associates a username
 * with a context ID.
 */
class UserJoin : public Message {
public:
	UserJoin(uint8_t ctx, const QByteArray &name) : Message(MSG_USER_JOIN, ctx), _name(name) {}
	UserJoin(uint8_t ctx, const QString &name) : UserJoin(ctx, name.toUtf8()) {}

	static UserJoin *deserialize(const uchar *data, uint len);

	QString name() const { return QString::fromUtf8(_name); }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	QByteArray _name;
};

/**
 * @brief Inform the client of a user leaving
 *
 * This message is sent only by the server. Upon receiving this message,
 * clients will typically remove the user from the user listing. The client
 * is also allowed to release resources associated with this context ID.
 */
class UserLeave : public Message {
public:
	explicit UserLeave(uint8_t ctx) : Message(MSG_USER_LEAVE, ctx) {}

	static UserLeave *deserialize(const uchar *data, uint len);

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;
};

/**
 * @brief User attribute change
 *
 * This message is sent only by the server. It is used to update user status
 * information. This information should only be used to update the user interface;
 * it should have no effect on the interpretation of drawing commands.
 */
class UserAttr: public Message {
public:
	static const uint16_t ATTR_LOCKED = 0x01; // user is locked
	static const uint16_t ATTR_OP = 0x02;     // user is a session operator
	static const uint16_t ATTR_MOD = 0x04;    // user is a moderator
	static const uint16_t ATTR_AUTH = 0x08;   // authenticated user (not a guest)

	UserAttr(uint8_t ctx, uint16_t attrs) : Message(MSG_USER_ATTR, ctx), _attrs(attrs) {}
	UserAttr(uint8_t ctx, bool locked, bool op, bool mod, bool auth)
		: UserAttr(ctx, (locked?ATTR_LOCKED:0) | (op?ATTR_OP:0) | (mod?ATTR_MOD:0) | (auth?ATTR_AUTH:0))
		{}

	static UserAttr *deserialize(const uchar *data, uint len);

	uint8_t attrs() const { return _attrs; }

	bool isLocked() const { return _attrs & ATTR_LOCKED; }
	bool isOp() const { return _attrs & ATTR_OP; }
	bool isMod() const { return _attrs & ATTR_MOD; }
	bool isAuth() const { return _attrs & ATTR_AUTH; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint16_t _attrs;
};

/**
 * @brief Change the session title
 *
 */
class SessionTitle : public Message {
public:
	SessionTitle(uint8_t ctx, const QByteArray &title) : Message(MSG_SESSION_TITLE, ctx), _title(title) {}
	SessionTitle(uint8_t ctx, const QString &title) : SessionTitle(ctx, title.toUtf8()) {}

	static SessionTitle *deserialize(const uchar *data, uint len);

	QString title() const { return QString::fromUtf8(_title); }

	bool isOpCommand() const { return true; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	QByteArray _title;
};

/**
 * @brief Update session configuration information
 *
 * This message is sent only by the server. As with the user status update,
 * this information should only be used to update the user interface.
 */
class SessionConf : public Message {
public:
	static const uint16_t ATTR_LOCKED = 0x01;
	static const uint16_t ATTR_CLOSED = 0x02;
	static const uint16_t ATTR_LAYERCTRLLOCKED = 0x04;
	static const uint16_t ATTR_LOCKDEFAULT = 0x08;
	static const uint16_t ATTR_PERSISTENT = 0x10;
	static const uint16_t ATTR_PRESERVECHAT = 0x20;
	static const uint16_t ATTR_LOCKIMAGE = 0x40;

	SessionConf(uint8_t maxusers, uint16_t attrs) : Message(MSG_SESSION_CONFIG, 0), _maxusers(maxusers), _attrs(attrs) {}
	SessionConf(uint8_t maxusers, bool locked, bool closed, bool layerctrlslocked, bool lockdefault, bool persistent, bool preservechat, bool putimagelocked)
		: SessionConf(
			  maxusers,
			  (locked?ATTR_LOCKED:0) |
			  (closed?ATTR_CLOSED:0) |
			  (layerctrlslocked?ATTR_LAYERCTRLLOCKED:0) |
			  (lockdefault?ATTR_LOCKDEFAULT:0) |
			  (persistent?ATTR_PERSISTENT:0) |
			  (preservechat?ATTR_PRESERVECHAT:0) |
			  (putimagelocked?ATTR_LOCKIMAGE:0)
		) {}

	static SessionConf *deserialize(const uchar *data, uint len);

	//! The maximum number of users in the session
	uint8_t maxUsers() const { return _maxusers; }

	uint16_t attrs() const { return _attrs; }

	//! Is the whole session locked?
	bool isLocked() const { return _attrs & ATTR_LOCKED; }

	//! Is the session closed to further users?
	bool isClosed() const { return _attrs & ATTR_CLOSED; }

	//! Are layer controls limited to operators only?
	bool isLayerControlsLocked() const { return _attrs & ATTR_LAYERCTRLLOCKED; }

	//! Is PutImage and FillRect locked?
	bool isPutImageLocked() const { return _attrs & ATTR_LOCKIMAGE; }

	//! Are new users locked automatically when they join?
	bool isUsersLockedByDefault() const { return _attrs & ATTR_LOCKDEFAULT; }

	//! Is this a persistent session?
	bool isPersistent() const { return _attrs & ATTR_PERSISTENT; }

	//! Are normal chat messages preserved in the session history?
	bool isChatPreserved() const { return _attrs & ATTR_PRESERVECHAT; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _maxusers;
	uint16_t _attrs;
};

/**
 * @brief A chat message
 *
 * Chat message sent by the server with the context ID 0 are server messages.
 */
class Chat : public Message {
public:
	static const uint8_t FLAG_ANNOUNCE = 0x01; // public announcement are included in the session history
	static const uint8_t FLAG_OPCMD = 0x02;    // this message is an operator command
	static const uint8_t FLAG_ACTION = 0x04;   // this is an "action message" (like /me in IRC)
	static const uint8_t FLAG_PIN = 0x08;      // pin this message

	Chat(uint8_t ctx, uint8_t flags, const QByteArray &msg) : Message(MSG_CHAT, ctx), _flags(flags), _msg(msg) {}
	Chat(uint8_t ctx, const QString &msg, bool publicAnnouncement, bool action)
		: Chat(
			ctx,
			(publicAnnouncement ? FLAG_ANNOUNCE : 0) |
			(action ? FLAG_ACTION : 0),
			msg.toUtf8()
			) {}

	//! Construct a chat message that carries a session operator command
	static MessagePtr opCommand(uint8_t ctx, const QString &cmd) { return MessagePtr(new Chat(ctx, FLAG_OPCMD, cmd.toUtf8())); }

	//! Construct a pinned message
	static MessagePtr pin(uint8_t ctx, const QString &message) { return MessagePtr(new Chat(ctx, FLAG_ANNOUNCE|FLAG_PIN, message.toUtf8())); }

	static Chat *deserialize(const uchar *data, uint len);

	uint8_t flags() const { return _flags; }

	QString message() const { return QString::fromUtf8(_msg); }

	/**
	 * @brief Is this a public announcement?
	 *
	 * Unlike normal chat messages, public announcements are included in the session history
	 * and will thus be visible to users who join later.
	 */
	bool isAnnouncement() const { return _flags & FLAG_ANNOUNCE; }

	/**
	 * @brief Is this an action message?
	 */
	bool isAction() const { return _flags & FLAG_ACTION; }

	/**
	 * @brief Is this chat message an operator command?
	 */
	bool isOpCommand() const { return _flags & FLAG_OPCMD; }

	/**
	 * @brief Is this a pinned message?
	 *
	 * Only session owners may pin messages.
	 */
	bool isPinned() const { return _flags & FLAG_PIN; }

protected:
    int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _flags;
    QByteArray _msg;
};

/**
 * @brief Move user pointer
 *
 * This is message is used to update the position of the user pointer when no
 * actual drawing is taking place. It is also used to draw the "laser pointer" trail.
 * Note. This is a META message, since this is used for a temporary visual effect only,
 * and thus doesn't affect the actual canvas content.
 *
 * When persistence is nonzero, a line is drawn from the previous pointer coordinates
 * to the new coordinates. The line will disappear in p seconds.
 */
class MovePointer : public Message {
public:
	MovePointer(uint8_t ctx, int32_t x, int32_t y, uint8_t persistence)
		: Message(MSG_MOVEPOINTER, ctx), _x(x), _y(y), _persistence(persistence)
	{}

	static MovePointer *deserialize(const uchar *data, uint len);

	int32_t x() const { return _x; }
	int32_t y() const { return _y; }
	uint8_t persistence() const { return _persistence; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	int32_t _x;
	int32_t _y;
	uint8_t _persistence;
};

}

#endif
