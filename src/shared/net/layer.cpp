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
	if(len!=4)
		return 0;
	return new CanvasResize(
		qFromBigEndian<quint16>(data),
		qFromBigEndian<quint16>(data+2)
	);
}

int CanvasResize::payloadLength() const
{
	return 4;
}

int CanvasResize::serializePayload(uchar *data) const
{
	qToBigEndian(_width, data); data += 2;
	qToBigEndian(_height, data);
	return 4;
}

LayerCreate *LayerCreate::deserialize(const uchar *data, uint len)
{
	if(len<6)
		return 0;

	return new LayerCreate(
		*data,
		*(data+1),
		qFromBigEndian<quint32>(data+2),
		QByteArray((const char*)data+6, len-6)
	);
}

int LayerCreate::payloadLength() const
{
	return 6 + _title.length();
}

int LayerCreate::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = _ctxid;
	*(ptr++) = _id;
	qToBigEndian(_fill, ptr); ptr += 4;
	memcpy(ptr, _title.constData(), _title.length());
	ptr += _title.length();
	return ptr - data;
}

LayerAttributes *LayerAttributes::deserialize(const uchar *data, uint len)
{
	if(len!=3)
		return 0;
	return new LayerAttributes(
		*data,
		*(data+1),
		*(data+2)
	);
}

int LayerAttributes::payloadLength() const
{
	return 3;
}


int LayerAttributes::serializePayload(uchar *data) const
{
	uchar *ptr=data;
	*(ptr++) = _id;
	*(ptr++) = _opacity;
	*(ptr++) = _blend;
	return ptr-data;
}
LayerRetitle *LayerRetitle::deserialize(const uchar *data, uint len)
{
	if(len<1)
		return 0;
	return new LayerRetitle(
		*data,
		QByteArray((const char*)data+1,len-1)
	);
}

int LayerRetitle::payloadLength() const
{
	return 1+_title.length();
}


int LayerRetitle::serializePayload(uchar *data) const
{
	*data = _id;
	memcpy(data+1, _title.constData(), _title.length());
	return 1+_title.length();
}

LayerOrder *LayerOrder::deserialize(const uchar *data, uint len)
{
	if(len==0 || len>255)
		return 0;

	QList<uint8_t> order;
	order.reserve(len);
	for(uint i=0;i<len;++i)
		order.append(data[i]);

	return new LayerOrder(order);
}

int LayerOrder::payloadLength() const
{
	return _order.size();
}

int LayerOrder::serializePayload(uchar *data) const
{
	Q_ASSERT(_order.length()<256);
	foreach(uint8_t l, _order)
		*(data++) = l;
	return _order.length();
}

LayerDelete *LayerDelete::deserialize(const uchar *data, uint len)
{
	if(len != 2)
		return 0;
	return new LayerDelete(data[0], data[1]);
}

int LayerDelete::payloadLength() const
{
	return 2;
}

int LayerDelete::serializePayload(uchar *data) const
{
	*data = _id; ++data;
	*data = _merge;
	return 2;
}

LayerACL *LayerACL::deserialize(const uchar *data, uint len)
{
	if(len < 2 || len > 2+255)
		return 0;
	uint8_t id = data[0];
	uint8_t lock = data[1];
	QList<uint8_t> exclusive;
	for(uint i=2;i<len;++i)
		exclusive.append(data[i]);

	return new LayerACL(id, lock, exclusive);
}

int LayerACL::payloadLength() const
{
	return 2 + _exclusive.count();
}

int LayerACL::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = _id;
	*(ptr++) = _locked;
	foreach(uint8_t e, _exclusive)
		*(ptr++) = e;
	return ptr-data;
}

}
