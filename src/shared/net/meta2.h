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
#ifndef DP_NET_META_OPAQUE_H
#define DP_NET_META_OPAQUE_H

#include "message.h"

#include <QString>
#include <QList>

namespace protocol {

/**
 * @brief Start/end drawing pointer laser trail
 *
 * This signals the beginning or the end of a laser pointer trail. The trail coordinates
 * are sent with MovePointer messages.
 *
 * A nonzero persistence indicates the start of the trail and zero the end.
 */
class LaserTrail : public Message {
public:
	LaserTrail(uint8_t ctx, quint32 color, uint8_t persistence) : Message(MSG_LASERTRAIL, ctx), m_color(color), m_persistence(persistence) { }
	static LaserTrail *deserialize(uint8_t ctx, const uchar *data, uint len);
	static LaserTrail *fromText(uint8_t ctx, const Kwargs &kwargs);

	quint32 color() const { return m_color; }
	uint8_t persistence() const { return m_persistence; }

	QString messageName() const override { return QStringLiteral("laser"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	quint32 m_color;
	uint8_t m_persistence;
};

/**
 * @brief Move user pointer
 *
 * This is message is used to update the position of the user pointer when no
 * actual drawing is taking place. It is also used to draw the "laser pointer" trail.
 * Note. This is a META message, since this is used for a temporary visual effect only,
 * and thus doesn't affect the actual canvas content.
 */
class MovePointer : public Message {
public:
	MovePointer(uint8_t ctx, int32_t x, int32_t y)
		: Message(MSG_MOVEPOINTER, ctx), m_x(x), m_y(y)
	{}

	static MovePointer *deserialize(uint8_t ctx, const uchar *data, uint len);
	static MovePointer *fromText(uint8_t ctx, const Kwargs &kwargs);

	int32_t x() const { return m_x; }
	int32_t y() const { return m_y; }

	QString messageName() const override { return QStringLiteral("movepointer"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	int32_t m_x;
	int32_t m_y;
};

/**
 * @brief Set user specific locks
 *
 * This is an opaque meta command that contains a list of users to be locked.
 * It can only be sent by session operators.
 */
class UserACL : public Message {
public:
	UserACL(uint8_t ctx, QList<uint8_t> ids) : Message(MSG_USER_ACL, ctx), m_ids(ids) { }

	static UserACL *deserialize(uint8_t ctx, const uchar *data, uint len);
	static UserACL *fromText(uint8_t ctx, const Kwargs &kwargs);

	bool isOpCommand() const { return true; }

	QList<uint8_t> ids() const { return m_ids; }

	QString messageName() const override { return QStringLiteral("useracl"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	QList<uint8_t> m_ids;
};

/**
 * @brief Change layer access control list
 *
 * This is an opaque meta command. It is used to set the general layer lock
 * as well as give exclusive access to selected users.
 *
 * When the OWNLAYERS mode is set, any user can use this to change the ACLs on layers they themselves
 * have created (identified by the ID prefix.)
 */
class LayerACL : public Message {
public:
	LayerACL(uint8_t ctx, uint16_t id, uint8_t locked, const QList<uint8_t> &exclusive)
		: Message(MSG_LAYER_ACL, ctx), m_id(id), m_locked(locked), m_exclusive(exclusive)
	{}

	static LayerACL *deserialize(uint8_t ctx, const uchar *data, uint len);
	static LayerACL *fromText(uint8_t ctx, const Kwargs &kwargs);

	// Note: this is an operator only command, depending on the target layer and whether OWNLAYERS mode is set.

	uint16_t id() const { return m_id; }
	uint8_t locked() const { return m_locked; }
	const QList<uint8_t> exclusive() const { return m_exclusive; }

	QString messageName() const override { return QStringLiteral("layeracl"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_id;
	uint8_t m_locked;
	QList<uint8_t> m_exclusive;
};

/**
 * @brief Change session wide access control settings
 *
 * This is an opaque meta command.
 */
class SessionACL : public Message {
public:
	static const uint16_t LOCK_SESSION = 0x01;   // General session-wide lock (locks even operators)
	static const uint16_t LOCK_DEFAULT = 0x02;   // New users will be locked by default (lock applied when the JOIN message is received)
	static const uint16_t LOCK_LAYERCTRL = 0x04; // Layer controls are limited to session operators
	static const uint16_t LOCK_OWNLAYERS = 0x08; // Users can only delete/adjust their own layers. (May set layer ACLs too)
	static const uint16_t LOCK_IMAGES = 0x10;    // PutImage and FillRect commands (and features that use them) are limited to session operators
	static const uint16_t LOCK_ANNOTATIONS = 0x20; // Only operators can create new annotations

	SessionACL(uint8_t ctx, uint16_t flags) : Message(MSG_SESSION_ACL, ctx), m_flags(flags) {}

	static SessionACL *deserialize(uint8_t ctx, const uchar *data, uint len);
	static SessionACL *fromText(uint8_t ctx, const Kwargs &kwargs);

	bool isOpCommand() const { return true; }

	uint16_t flags() const { return m_flags; }

	bool isSessionLocked() const { return m_flags & LOCK_SESSION; }
	bool isLockedByDefault() const { return m_flags & LOCK_DEFAULT; }
	bool isLayerControlLocked() const { return m_flags & LOCK_LAYERCTRL; }
	bool isOwnLayers() const { return m_flags & LOCK_OWNLAYERS; }
	bool isImagesLocked() const { return m_flags & LOCK_IMAGES; }
	bool isAnnotationCreationLocked() const { return m_flags & LOCK_ANNOTATIONS; }

	QString messageName() const override { return QStringLiteral("sessionacl"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_flags;
};

/**
 * @brief Set the default layer
 *
 * The default layer is the one new users default to when logging in.
 * If no default layer is set, the newest layer will be selected by default.
 */
class DefaultLayer : public Message {
public:
	DefaultLayer(uint8_t ctx, uint16_t id)
		: Message(MSG_LAYER_DEFAULT, ctx),
		m_id(id)
		{}

	static DefaultLayer *deserialize(uint8_t ctx, const uchar *data, uint len);
	static DefaultLayer *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint16_t id() const { return m_id; }

	bool isOpCommand() const { return true; }

	QString messageName() const override { return QStringLiteral("defaultlayer"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_id;
};


}

#endif
