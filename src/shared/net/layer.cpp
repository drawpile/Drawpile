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

#include <QtEndian>
#include "layer.h"

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
	qToBigEndian(_top, ptr); ptr += 4;
	qToBigEndian(_right, ptr); ptr += 4;
	qToBigEndian(_bottom, ptr); ptr += 4;
	qToBigEndian(_left, ptr); ptr += 4;
	return ptr - data;
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
	return 9 + _title.length();
}

int LayerCreate::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(_id, ptr); ptr += 2;
	qToBigEndian(_source, ptr); ptr += 2;
	qToBigEndian(_fill, ptr); ptr += 4;
	*(ptr++) = _flags;
	memcpy(ptr, _title.constData(), _title.length());
	ptr += _title.length();
	return ptr - data;
}

LayerAttributes *LayerAttributes::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=4)
		return 0;
	return new LayerAttributes(
		ctx,
		qFromBigEndian<quint16>(data+0),
		*(data+2),
		*(data+3)
	);
}

int LayerAttributes::payloadLength() const
{
	return 4;
}


int LayerAttributes::serializePayload(uchar *data) const
{
	uchar *ptr=data;
	qToBigEndian(_id, ptr); ptr += 2;
	*(ptr++) = _opacity;
	*(ptr++) = _blend;
	return ptr-data;
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
	return 2 + _title.length();
}


int LayerRetitle::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(_id, ptr); ptr += 2;
	memcpy(ptr, _title.constData(), _title.length());
	ptr += _title.length();
	return ptr - data;
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
	return _order.size() * 2;
}

int LayerOrder::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	foreach(uint16_t l, _order) {
		qToBigEndian(l, ptr); ptr += 2;
	}
	return ptr - data;
}

QList<uint16_t> LayerOrder::sanitizedOrder(const QList<uint16_t> &currentOrder) const
{
	QList<uint16_t> S;
	S.reserve(currentOrder.size());

	// remove duplicates and IDs not found in the current order
	for(uint16_t l : _order) {
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
	qToBigEndian(_id, ptr); ptr += 2;
	*(ptr++) = _merge;
	return ptr - data;
}

}
