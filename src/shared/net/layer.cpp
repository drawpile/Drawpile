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

CanvasResize *CanvasResize::deserialize(const uchar *data, uint len)
{
	if(len!=17)
		return 0;
	return new CanvasResize(
		*data,
		qFromBigEndian<qint32>(data+1),
		qFromBigEndian<qint32>(data+5),
		qFromBigEndian<qint32>(data+9),
		qFromBigEndian<qint32>(data+13)
	);
}

int CanvasResize::payloadLength() const
{
	return 1 + 4*4;
}

int CanvasResize::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	qToBigEndian(_top, ptr); ptr += 4;
	qToBigEndian(_right, ptr); ptr += 4;
	qToBigEndian(_bottom, ptr); ptr += 4;
	qToBigEndian(_left, ptr); ptr += 4;
	return ptr - data;
}

LayerCreate *LayerCreate::deserialize(const uchar *data, uint len)
{
	if(len<10)
		return 0;

	return new LayerCreate(
		*(data+0),
		qFromBigEndian<quint16>(data+1),
		qFromBigEndian<quint16>(data+3),
		qFromBigEndian<quint32>(data+5),
		*(data+9),
		QByteArray((const char*)data+10, len-10)
	);
}

int LayerCreate::payloadLength() const
{
	return 1 + 9 + _title.length();
}

int LayerCreate::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	qToBigEndian(_id, ptr); ptr += 2;
	qToBigEndian(_source, ptr); ptr += 2;
	qToBigEndian(_fill, ptr); ptr += 4;
	*(ptr++) = _flags;
	memcpy(ptr, _title.constData(), _title.length());
	ptr += _title.length();
	return ptr - data;
}

LayerAttributes *LayerAttributes::deserialize(const uchar *data, uint len)
{
	if(len!=5)
		return 0;
	return new LayerAttributes(
		*(data+0),
		qFromBigEndian<quint16>(data+1),
		*(data+3),
		*(data+4)
	);
}

int LayerAttributes::payloadLength() const
{
	return 5;
}


int LayerAttributes::serializePayload(uchar *data) const
{
	uchar *ptr=data;
	*(ptr++) = contextId();
	qToBigEndian(_id, ptr); ptr += 2;
	*(ptr++) = _opacity;
	*(ptr++) = _blend;
	return ptr-data;
}
LayerRetitle *LayerRetitle::deserialize(const uchar *data, uint len)
{
	if(len<3)
		return 0;
	return new LayerRetitle(
		*(data+0),
		qFromBigEndian<quint16>(data+1),
		QByteArray((const char*)data+3,len-3)
	);
}

int LayerRetitle::payloadLength() const
{
	return 1 + 2 + _title.length();
}


int LayerRetitle::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	qToBigEndian(_id, ptr); ptr += 2;
	memcpy(ptr, _title.constData(), _title.length());
	ptr += _title.length();
	return ptr - data;
}

LayerOrder *LayerOrder::deserialize(const uchar *data, uint len)
{
	if(len<1)
		return 0;

	if((len%2) == 0)
		return 0;

	QList<uint16_t> order;
	order.reserve((len-1) / 2);
	for(uint i=1;i<len;i+=2)
		order.append(qFromBigEndian<quint16>(data+i));

	return new LayerOrder(data[0], order);
}

int LayerOrder::payloadLength() const
{
	return 1 + _order.size() * 2;
}

int LayerOrder::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
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

LayerDelete *LayerDelete::deserialize(const uchar *data, uint len)
{
	if(len != 4)
		return 0;
	return new LayerDelete(
		data[0],
		qFromBigEndian<quint16>(data+1),
		data[3]
	);
}

int LayerDelete::payloadLength() const
{
	return 1 + 2 + 1;
}

int LayerDelete::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	qToBigEndian(_id, ptr); ptr += 2;
	*(ptr++) = _merge;
	return ptr - data;
}

LayerACL *LayerACL::deserialize(const uchar *data, uint len)
{
	if(len < 4 || len > 4+255)
		return 0;
	uint8_t ctx = data[0];
	uint16_t id = qFromBigEndian<quint16>(data+1);
	uint8_t lock = data[3];
	QList<uint8_t> exclusive;
	for(uint i=4;i<len;++i)
		exclusive.append(data[i]);

	return new LayerACL(ctx, id, lock, exclusive);
}

int LayerACL::payloadLength() const
{
	return 1 + 3 + _exclusive.count();
}

int LayerACL::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	qToBigEndian(_id, ptr); ptr += 2;
	*(ptr++) = _locked;
	foreach(uint8_t e, _exclusive)
		*(ptr++) = e;
	return ptr-data;
}

}
