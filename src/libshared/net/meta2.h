/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2018 Calle Laakkonen

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

#include "libshared/net/message.h"

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
 *
 * Using layer ID 0 sets or clears a general canvaswide lock. The tier and exclusive user list is not
 * used in this case.
 */
class LayerACL : public Message {
public:
	LayerACL(uint8_t ctx, uint16_t id, bool locked, uint8_t tier, const QList<uint8_t> &exclusive)
		: LayerACL(ctx, id, (locked?0x80:0) | (tier&0x07), exclusive)
	{}

	static LayerACL *deserialize(uint8_t ctx, const uchar *data, uint len);
	static LayerACL *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint16_t layer() const override { return m_id; }
	bool locked() const { return m_flags & 0x80; }
	uint8_t tier() const { return m_flags & 0x07;}
	const QList<uint8_t> exclusive() const { return m_exclusive; }

	QString messageName() const override { return QStringLiteral("layeracl"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	LayerACL(uint8_t ctx, uint16_t id, uint8_t flags, const QList<uint8_t> &exclusive)
		: Message(MSG_LAYER_ACL, ctx), m_id(id), m_flags(flags), m_exclusive(exclusive)
	{}

	uint16_t m_id;
	uint8_t m_flags;
	QList<uint8_t> m_exclusive;
};

/**
 * @brief Change feature access levels
 *
 * This is an opaque meta command.
 */
class FeatureAccessLevels : public Message {
public:
	static const int FEATURES = 9; // Number of configurable features

	FeatureAccessLevels(uint8_t ctx, const uint8_t *featureTiers)
		: Message(MSG_FEATURE_LEVELS, ctx)
	{
		for(int i=0;i<FEATURES;++i)
			m_featureTiers[i] = featureTiers[i];
	}

	static FeatureAccessLevels *deserialize(uint8_t ctx, const uchar *data, uint len);
	static FeatureAccessLevels *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint8_t featureTier(int featureIdx) const { Q_ASSERT(featureIdx>=0 && featureIdx<FEATURES); return m_featureTiers[featureIdx]; }

	QString messageName() const override { return QStringLiteral("featureaccess"); }

protected:
	int payloadLength() const override { return FEATURES; }
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint8_t m_featureTiers[FEATURES];
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

	uint16_t layer() const override { return m_id; }

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
