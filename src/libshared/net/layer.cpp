/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2019 Calle Laakkonen

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

#include "libshared/net/layer.h"
#include "libshared/net/textmode.h"

#include <QtEndian>

namespace protocol {

CanvasResize *CanvasResize::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=16)
		return 0;
	return new CanvasResize(
		ctx,
		qFromBigEndian<qint32>(data+0),
		qFromBigEndian<qint32>(data+4),
		qFromBigEndian<qint32>(data+8),
		qFromBigEndian<qint32>(data+12)
	);
}

int CanvasResize::payloadLength() const
{
	return 4*4;
}

int CanvasResize::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(m_top, ptr); ptr += 4;
	qToBigEndian(m_right, ptr); ptr += 4;
	qToBigEndian(m_bottom, ptr); ptr += 4;
	qToBigEndian(m_left, ptr); ptr += 4;
	return ptr - data;
}

Kwargs CanvasResize::kwargs() const
{
	Kwargs kw;
	if(m_top)
		kw["top"] = QString::number(m_top);
	if(m_right)
		kw["right"] = QString::number(m_right);
	if(m_bottom)
		kw["bottom"] = QString::number(m_bottom);
	if(m_left)
		kw["left"] = QString::number(m_left);
	return kw;
}

CanvasResize *CanvasResize::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new CanvasResize(
		ctx,
		kwargs["top"].toInt(),
		kwargs["right"].toInt(),
		kwargs["bottom"].toInt(),
		kwargs["left"].toInt()
		);
}

LayerCreate *LayerCreate::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len<9)
		return 0;

	return new LayerCreate(
		ctx,
		qFromBigEndian<quint16>(data+0),
		qFromBigEndian<quint16>(data+2),
		qFromBigEndian<quint32>(data+4),
		*(data+8),
		QByteArray((const char*)data+9, len-9)
	);
}

int LayerCreate::payloadLength() const
{
	return 9 + m_title.length();
}

int LayerCreate::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(m_id, ptr); ptr += 2;
	qToBigEndian(m_source, ptr); ptr += 2;
	qToBigEndian(m_fill, ptr); ptr += 4;
	*(ptr++) = m_flags;
	memcpy(ptr, m_title.constData(), m_title.length());
	ptr += m_title.length();
	return ptr - data;
}

Kwargs LayerCreate::kwargs() const
{
	Kwargs kw;
	kw["id"] = text::idString(m_id);
	if(m_source)
		kw["source"] = text::idString(m_source);
	if(m_fill)
		kw["fill"] = text::argbString(m_fill);
	QStringList flags;
	if((m_flags&FLAG_COPY))
		flags << "copy";
	if((m_flags&FLAG_INSERT))
		flags << "insert";
	if(!flags.isEmpty())
		kw["flags"] = flags.join(',');
	kw["title"] = title();
	return kw;
}

LayerCreate *LayerCreate::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	QStringList flags = kwargs["flags"].split(',');
	return new LayerCreate(
		ctx,
		text::parseIdString16(kwargs["id"]),
		text::parseIdString16(kwargs["source"]),
		text::parseColor(kwargs["fill"]),
		(flags.contains("copy") ? FLAG_COPY : 0) |
		(flags.contains("insert") ? FLAG_INSERT : 0),
		kwargs["title"]
		);
}

LayerAttributes *LayerAttributes::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=6)
		return nullptr;
	return new LayerAttributes(
		ctx,
		qFromBigEndian<quint16>(data+0),
		*(data+2),
		*(data+3),
		*(data+4),
		*(data+5)
	);
}

int LayerAttributes::payloadLength() const
{
	return 6;
}


int LayerAttributes::serializePayload(uchar *data) const
{
	uchar *ptr=data;
	qToBigEndian(m_id, ptr); ptr += 2;
	*(ptr++) = m_sublayer;
	*(ptr++) = m_flags;
	*(ptr++) = m_opacity;
	*(ptr++) = m_blend;
	return ptr-data;
}

Kwargs LayerAttributes::kwargs() const
{
	Kwargs kw;
	kw["layer"] = text::idString(m_id);
	if(m_sublayer>0)
		kw["sublayer"] = QString::number(m_sublayer);
	kw["opacity"] = text::decimal(m_opacity);
	kw["blend"] = QString::number(m_blend);

	QStringList flags;
	if(isCensored())
		flags << "censor";
	if(isFixed())
		flags << "fixed";
	if(!flags.isEmpty())
		kw["flags"] = flags.join(',');

	return kw;
}

LayerAttributes *LayerAttributes::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	QStringList flags = kwargs["flags"].split(',');
	return new LayerAttributes(
		ctx,
		text::parseIdString16(kwargs["layer"]),
		kwargs["sublayer"].toInt(),
		(flags.contains("censor") ? FLAG_CENSOR : 0) |
		(flags.contains("fixed") ? FLAG_FIXED : 0),
		text::parseDecimal8(kwargs["opacity"]),
		kwargs["blend"].toInt()
		);
}

LayerVisibility *LayerVisibility::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=3)
		return 0;
	return new LayerVisibility(
		ctx,
		qFromBigEndian<quint16>(data+0),
		*(data+2)
	);
}

int LayerVisibility::payloadLength() const
{
	return 3;
}


int LayerVisibility::serializePayload(uchar *data) const
{
	uchar *ptr=data;
	qToBigEndian(m_id, ptr); ptr += 2;
	*(ptr++) = m_visible;
	return ptr-data;
}

Kwargs LayerVisibility::kwargs() const
{
	Kwargs kw;
	kw["id"] = text::idString(m_id);
	kw["visible"] = m_visible ? "true" : "false";
	return kw;
}

LayerVisibility *LayerVisibility::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new LayerVisibility(
		ctx,
		text::parseIdString16(kwargs["id"]),
		kwargs["visible"] == "true"
		);
}

LayerRetitle *LayerRetitle::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len<2)
		return 0;
	return new LayerRetitle(
		ctx,
		qFromBigEndian<quint16>(data+0),
		QByteArray((const char*)data+2,len-2)
	);
}

int LayerRetitle::payloadLength() const
{
	return 2 + m_title.length();
}


int LayerRetitle::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(m_id, ptr); ptr += 2;
	memcpy(ptr, m_title.constData(), m_title.length());
	ptr += m_title.length();
	return ptr - data;
}

Kwargs LayerRetitle::kwargs() const
{
	Kwargs kw;
	kw["id"] = text::idString(m_id);
	kw["title"] = title();
	return kw;
}

LayerRetitle *LayerRetitle::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new LayerRetitle(
		ctx,
		text::parseIdString16(kwargs["id"]),
		kwargs["title"]
		);
}

LayerOrder *LayerOrder::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if((len%2) != 0)
		return nullptr;

	QList<uint16_t> order;
	order.reserve(len / 2);
	for(uint i=0;i<len;i+=2)
		order.append(qFromBigEndian<quint16>(data+i));

	return new LayerOrder(ctx, order);
}

int LayerOrder::payloadLength() const
{
	return m_order.size() * 2;
}

int LayerOrder::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	for(uint16_t l : m_order) {
		qToBigEndian(l, ptr); ptr += 2;
	}
	return ptr - data;
}

QList<uint16_t> LayerOrder::sanitizedOrder(const QList<uint16_t> &currentOrder) const
{
	QList<uint16_t> S;
	S.reserve(currentOrder.size());

	// remove duplicates and IDs not found in the current order
	for(uint16_t l : m_order) {
		if(!S.contains(l) && currentOrder.contains(l))
			S.append(l);
	}

	// at this point, S contains no duplicate items and no items not in currentOrder
	// therefore |S| <= |currentOrder|

	// add leftover IDs from currentOrder
	int i=0;
	while(S.size() < currentOrder.size()) {
		if(!S.contains(currentOrder.at(i)))
			S.append(currentOrder.at(i));
		++i;
	}

	// the above loop ends when |S| == |currentOrder|. Since S may not contain duplicates
	// or items not in currentOrder, S must be a permutation of currentOrder.

	return S;
}

Kwargs LayerOrder::kwargs() const
{
	Kwargs kw;
	kw["layers"] = text::idListString(m_order);
	return kw;
}

LayerOrder *LayerOrder::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new LayerOrder(
		ctx,
		text::parseIdListString16(kwargs["layers"])
		);
}

LayerDelete *LayerDelete::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len != 3)
		return nullptr;
	return new LayerDelete(
		ctx,
		qFromBigEndian<quint16>(data+0),
		data[2]
	);
}

int LayerDelete::payloadLength() const
{
	return 2 + 1;
}

int LayerDelete::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(m_id, ptr); ptr += 2;
	*(ptr++) = m_merge;
	return ptr - data;
}

Kwargs LayerDelete::kwargs() const
{
	Kwargs kw;
	kw["id"] = text::idString(m_id);
	if(m_merge)
		kw["merge"] = "true";
	return kw;
}

LayerDelete *LayerDelete::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new LayerDelete(
		ctx,
		text::parseIdString16(kwargs["id"]),
		kwargs["merge"] == "true"
		);
}

}

