/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#include "pen.h"

namespace protocol {

ToolChange *ToolChange::deserialize(const uchar *data, uint len)
{
	if(len != 19)
		return 0;

	return new ToolChange(
		*(data+0),
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
	return 1 + 18;
}

int ToolChange::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	*(ptr++) = _layer;
	*(ptr++) = _blend;
	*(ptr++) = _mode;
	*(ptr++) = _spacing;
	qToBigEndian(_color_h, ptr); ptr += 4;
	qToBigEndian(_color_l, ptr); ptr += 4;
	*(ptr++) = _hard_h;
	*(ptr++) = _hard_l;
	*(ptr++) = _size_h;
	*(ptr++) = _size_l;
	*(ptr++) = _opacity_h;
	*(ptr++) = _opacity_l;
	return ptr-data;
}

PenMove *PenMove::deserialize(const uchar *data, uint len)
{
	if(len<11 || (len-1)%10)
		return 0;
	PenPointVector pp;

	uint8_t ctx = *(data++);

	int points = (len-1)/10;
	pp.reserve(points);
	while(points--) {
		pp.append(PenPoint(
			qFromBigEndian<qint32>(data),
			qFromBigEndian<qint32>(data+4),
			qFromBigEndian<quint16>(data+8)
		));
		data += 10;
	}
	return new PenMove(ctx, pp);
}

int PenMove::payloadLength() const
{
	return 1 + 10 * _points.size();
}

int PenMove::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	foreach(const PenPoint &p, _points) {
		qToBigEndian(p.x, ptr); ptr += 4;
		qToBigEndian(p.y, ptr); ptr += 4;
		qToBigEndian(p.p, ptr); ptr += 2;
	}
	return ptr - data;
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
	*data = contextId();
	return 1;
}

}
