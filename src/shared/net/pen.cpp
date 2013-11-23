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
#include "pen.h"

namespace protocol {

ToolChange *ToolChange::deserialize(const uchar *data, uint len)
{
	if(len != 19)
		return 0;

	return new ToolChange(
		*data,
		*(data+1),
		*(data+2),
		*(data+3),
		*(data+4),
		qFromBigEndian<quint32>(data+5),
		qFromBigEndian<quint32>(data+9),
		*(data+13),
		*(data+14),
		*(data+15),
		*(data+16),
		*(data+17),
		*(data+18)
	);
}

int ToolChange::payloadLength() const
{
	return 19;
}

int ToolChange::serializePayload(uchar *data) const
{
	*data = _id; ++data;
	*data = _layer; ++data;
	*data = _blend; ++data;
	*data = _mode; ++data;
	*data = _spacing; ++data;
	qToBigEndian(_color_h, data); data += 4;
	qToBigEndian(_color_l, data); data += 4;
	*data = _hard_h; ++data;
	*data = _hard_l; ++data;
	*data = _size_h; ++data;
	*data = _size_l; ++data;
	*data = _opacity_h; ++data;
	*data = _opacity_l;
	return 19;
}

PenMove *PenMove::deserialize(const uchar *data, uint len)
{
	if(len<6)
		return 0;
	uint8_t id = *data;
	PenPointVector pp;

	const int points = (len-1)/6;
	pp.reserve(points);
	++data;
	for(int i=0;i<points;++i) {
		pp.append(PenPoint(
			qFromBigEndian<quint16>(data),
			qFromBigEndian<quint16>(data+2),
			*(data+4)
		));
		data += 5;
	}
	return new PenMove(id, pp);
}

int PenMove::payloadLength() const
{
	return 1 + 5 * _points.size();
}

int PenMove::serializePayload(uchar *data) const
{
	*data = _ctx; ++data;
	foreach(const PenPoint &p, _points) {
		qToBigEndian(p.x, data); data += 2;
		qToBigEndian(p.y, data); data += 2;
		*data = p.p; ++data;
	}
	return 1 + 5 * _points.size();
}

PenUp *PenUp::deserialize(const uchar *data, uint len)
{
	if(len != 1)
		return 0;

	return new PenUp(*data);
}

int PenUp::payloadLength() const
{
	return 1;
}

int PenUp::serializePayload(uchar *data) const
{
	*data = _ctx;
	return 1;
}

}
