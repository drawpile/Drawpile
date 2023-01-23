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

#include "libshared/net/meta2.h"
#include "libshared/net/textmode.h"

#include <cstring>
#include <QtEndian>

namespace protocol {

LaserTrail *LaserTrail::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=5)
		return nullptr;
	return new LaserTrail(
		ctx,
		qFromBigEndian<quint32>(data+0),
		*(data+4)
	);
}

int LaserTrail::payloadLength() const
{
	return 4+1;
}

int LaserTrail::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(m_color, ptr); ptr += 4;
	*(ptr++) = m_persistence;

	return ptr-data;
}

Kwargs LaserTrail::kwargs() const
{
	Kwargs kw;
	kw["color"] = text::rgbString(m_color);
	kw["persistence"] = QString::number(m_persistence);
	return kw;
}

LaserTrail *LaserTrail::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new LaserTrail(
		ctx,
		text::parseColor(kwargs["color"]),
		kwargs["persistence"].toInt()
		);
}

MovePointer *MovePointer::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=8)
		return nullptr;
	return new MovePointer(
		ctx,
		qFromBigEndian<quint32>(data+0),
		qFromBigEndian<quint32>(data+4)
	);
}

int MovePointer::payloadLength() const
{
	return 2*4;
}

int MovePointer::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(m_x, ptr); ptr += 4;
	qToBigEndian(m_y, ptr); ptr += 4;

	return ptr-data;
}

Kwargs MovePointer::kwargs() const
{
	Kwargs kw;
	kw["x"] = QString::number(m_x);
	kw["y"] = QString::number(m_y);
	return kw;
}

MovePointer *MovePointer::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new MovePointer(
		ctx,
		kwargs["x"].toInt(),
		kwargs["y"].toInt()
		);
}

UserACL *UserACL::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len>255)
		return nullptr;

	QList<uint8_t> ids;
	ids.reserve(len);
	for(uint i=0;i<len;++i)
		ids.append(data[i]);

	return new UserACL(ctx, ids);
}

int UserACL::serializePayload(uchar *data) const
{
	for(int i=0;i<m_ids.size();++i)
		data[i] = m_ids[i];
	return m_ids.size();
}

int UserACL::payloadLength() const
{
	return m_ids.size();
}

Kwargs UserACL::kwargs() const
{
	Kwargs kw;
	if(!m_ids.isEmpty())
		kw["users"] = text::idListString(m_ids);
	return kw;
}

UserACL *UserACL::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new UserACL(
		ctx,
		text::parseIdListString8(kwargs["users"])
		);
}

LayerACL *LayerACL::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len < 3 || len > 3+255)
		return nullptr;
	uint16_t id = qFromBigEndian<quint16>(data+0);
	uint8_t lock = data[2];
	QList<uint8_t> exclusive;
	for(uint i=3;i<len;++i)
		exclusive.append(data[i]);

	return new LayerACL(ctx, id, lock, exclusive);
}

int LayerACL::payloadLength() const
{
	return 3 + m_exclusive.count();
}

int LayerACL::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(m_id, ptr); ptr += 2;
	*(ptr++) = m_flags;
	for(uint8_t e : m_exclusive)
		*(ptr++) = e;
	return ptr-data;
}

static const char *TIER_NAMES[4] = {
	"op", "trusted", "auth", "guest"
};

static int tierFromName(const QString &name)
{
	for(int i=0;i<4;++i)
		if(name == TIER_NAMES[i])
			return i;
	return 0;
}

static const char *tierName(int tier)
{
	return TIER_NAMES[qBound(0, tier, 3)];
}

Kwargs LayerACL::kwargs() const
{
	Kwargs kw;
	kw["id"] = text::idString(m_id);
	kw["locked"] = locked() ? "true" : "false";


	if(m_id > 0) {
		kw["tier"] = tierName(tier());
		if(!m_exclusive.isEmpty())
			kw["exclusive"] = text::idListString(m_exclusive);
	}

	return kw;
}

LayerACL *LayerACL::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new LayerACL(
		ctx,
		text::parseIdString16(kwargs["id"]),
		kwargs["locked"] == "true",
		tierFromName(kwargs["tier"]),
		text::parseIdListString8(kwargs["exclusive"])
		);
}

FeatureAccessLevels *FeatureAccessLevels::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len != FEATURES)
		return nullptr;

	return new FeatureAccessLevels(ctx, data);
}

int FeatureAccessLevels::serializePayload(uchar *data) const
{
	memcpy(data, m_featureTiers, FEATURES);
	return FEATURES;
}

static const char *FEATURE_NAMES[FeatureAccessLevels::FEATURES] = {
	"putimage",
	"regionmove",
	"resize",
	"background",
	"editlayers",
	"ownlayers",
	"createannotation",
	"laser",
	"undo"
};

Kwargs FeatureAccessLevels::kwargs() const
{
	Kwargs kw;
	for(int i=0;i<FEATURES;++i) {
		if(m_featureTiers[i] > 0)
			kw[FEATURE_NAMES[i]] = tierName(m_featureTiers[i]);
	}
	return kw;
}

FeatureAccessLevels *FeatureAccessLevels::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	uint8_t features[FEATURES];
	for(int i=0;i<FEATURES;++i) {
		features[i] = tierFromName(kwargs[FEATURE_NAMES[i]]);
	}

	return new FeatureAccessLevels(ctx, features);
}

DefaultLayer *DefaultLayer::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len != 2)
		return nullptr;
	return new DefaultLayer(
		ctx,
		qFromBigEndian<quint16>(data)
	);
}

int DefaultLayer::payloadLength() const
{
	return 2;
}

int DefaultLayer::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(m_id, ptr); ptr += 2;
	return ptr - data;
}

Kwargs DefaultLayer::kwargs() const
{
	Kwargs kw;
	kw["id"] = text::idString(m_id);
	return kw;
}

DefaultLayer *DefaultLayer::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new DefaultLayer(
		ctx,
		text::parseIdString16(kwargs["id"])
		);
}

}
