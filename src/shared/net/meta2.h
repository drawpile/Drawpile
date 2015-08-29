/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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
#ifndef DP_NET_META_OPAQUE_H
#define DP_NET_META_OPAQUE_H

#include <QString>

#include "message.h"

namespace protocol {

/**
 * @brief A (recorded) chat message
 *
 * Chat message sent by the server with the context ID 0 are server messages.
 */
class Chat : public Message {
public:
	static const uint8_t FLAG_ANNOUNCE = 0x01; // public announcement are included in the session history
	static const uint8_t FLAG_ACTION = 0x04;   // this is an "action message" (like /me in IRC)

	Chat(uint8_t ctx, uint8_t flags, const QByteArray &msg) : Message(MSG_CHAT, ctx), m_flags(flags), m_msg(msg) {}
	Chat(uint8_t ctx, const QString &msg, bool publicAnnouncement, bool action)
		: Chat(
			ctx,
			(publicAnnouncement ? FLAG_ANNOUNCE : 0) |
			(action ? FLAG_ACTION : 0),
			msg.toUtf8()
			) {}

	static Chat *deserialize(uint8_t ctx, const uchar *data, uint len);

	uint8_t flags() const { return m_flags; }

	QString message() const { return QString::fromUtf8(m_msg); }

	/**
	 * @brief Is this a public announcement?
	 *
	 * Unlike normal chat messages, public announcements are included in the session history
	 * and will thus be visible to users who join later.
	 */
	bool isAnnouncement() const { return m_flags & FLAG_ANNOUNCE; }

	/**
	 * @brief Is this an action message?
	 */
	bool isAction() const { return m_flags & FLAG_ACTION; }

protected:
    int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t m_flags;
	QByteArray m_msg;
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

	static MovePointer *deserialize(uint8_t ctx, const uchar *data, uint len);

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
