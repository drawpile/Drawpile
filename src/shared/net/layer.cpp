/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#include <QtEndian>
#include "layer.h"

namespace protocol {

CanvasResize *CanvasResize::deserialize(const uchar *data, uint len)
{
	if(len!=9)
		return 0;
	return new CanvasResize(
		*data,
		qFromBigEndian<quint16>(data+1),
		qFromBigEndian<quint16>(data+3),
		qFromBigEndian<quint16>(data+5),
		qFromBigEndian<quint16>(data+7)
	);
}

int CanvasResize::payloadLength() const
{
	return 9;
}

int CanvasResize::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	qToBigEndian(_top, ptr); ptr += 2;
	qToBigEndian(_right, ptr); ptr += 2;
	qToBigEndian(_bottom, ptr); ptr += 2;
	qToBigEndian(_left, ptr); ptr += 2;
	return ptr - data;
}

LayerCreate *LayerCreate::deserialize(const uchar *data, uint len)
{
	if(len<6)
		return 0;

	return new LayerCreate(
		*(data+0),
		*(data+1),
		qFromBigEndian<quint32>(data+2),
		QByteArray((const char*)data+6, len-6)
	);
}

int LayerCreate::payloadLength() const
{
	return 1 + 5 + _title.length();
}

int LayerCreate::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	*(ptr++) = _id;
	qToBigEndian(_fill, ptr); ptr += 4;
	memcpy(ptr, _title.constData(), _title.length());
	ptr += _title.length();
	return ptr - data;
}

LayerAttributes *LayerAttributes::deserialize(const uchar *data, uint len)
{
	if(len!=4)
		return 0;
	return new LayerAttributes(
		*(data+0),
		*(data+1),
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
	*(ptr++) = contextId();
	*(ptr++) = _id;
	*(ptr++) = _opacity;
	*(ptr++) = _blend;
	return ptr-data;
}
LayerRetitle *LayerRetitle::deserialize(const uchar *data, uint len)
{
	if(len<2)
		return 0;
	return new LayerRetitle(
		*(data+0),
		*(data+1),
		QByteArray((const char*)data+2,len-2)
	);
}

int LayerRetitle::payloadLength() const
{
	return 1 + 1 + _title.length();
}


int LayerRetitle::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	*(ptr++) = _id;
	memcpy(ptr, _title.constData(), _title.length());
	ptr += _title.length();
	return ptr - data;
}

LayerOrder *LayerOrder::deserialize(const uchar *data, uint len)
{
	if(len<2 || len>256)
		return 0;

	QList<uint8_t> order;
	order.reserve(len);
	for(uint i=1;i<len;++i)
		order.append(data[i]);

	return new LayerOrder(data[0], order);
}

int LayerOrder::payloadLength() const
{
	return 1 + _order.size();
}

int LayerOrder::serializePayload(uchar *data) const
{
	Q_ASSERT(_order.length()<256);
	uchar *ptr = data;
	*(ptr++) = contextId();
	foreach(uint8_t l, _order)
		*(ptr++) = l;
	return ptr - data;
}

LayerDelete *LayerDelete::deserialize(const uchar *data, uint len)
{
	if(len != 3)
		return 0;
	return new LayerDelete(data[0], data[1], data[2]);
}

int LayerDelete::payloadLength() const
{
	return 1 + 2;
}

int LayerDelete::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	*(ptr++) = _id;
	*(ptr++) = _merge;
	return ptr - data;
}

LayerACL *LayerACL::deserialize(const uchar *data, uint len)
{
	if(len < 3 || len > 3+255)
		return 0;
	uint8_t ctx = data[0];
	uint8_t id = data[1];
	uint8_t lock = data[2];
	QList<uint8_t> exclusive;
	for(uint i=3;i<len;++i)
		exclusive.append(data[i]);

	return new LayerACL(ctx, id, lock, exclusive);
}

int LayerACL::payloadLength() const
{
	return 1 + 2 + _exclusive.count();
}

int LayerACL::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	*(ptr++) = _id;
	*(ptr++) = _locked;
	foreach(uint8_t e, _exclusive)
		*(ptr++) = e;
	return ptr-data;
}

}
