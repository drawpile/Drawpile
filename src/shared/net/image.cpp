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
#include "image.h"

namespace protocol {

PutImage *PutImage::deserialize(const uchar *data, uint len)
{
	if(len < 20)
		return 0;

	return new PutImage(
		*(data+0),
		qFromBigEndian<quint16>(data+1),
		*(data+3),
		qFromBigEndian<quint32>(data+4),
		qFromBigEndian<quint32>(data+8),
		qFromBigEndian<quint32>(data+12),
		qFromBigEndian<quint32>(data+16),
		QByteArray((const char*)data+20, len-20)
	);
}

int PutImage::payloadLength() const
{
	return 1 + 3 + 4*4 + _image.size();
}

int PutImage::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	qToBigEndian(_layer, ptr); ptr += 2;
	*(ptr++) = _mode;
	qToBigEndian(_x, ptr); ptr += 4;
	qToBigEndian(_y, ptr); ptr += 4;
	qToBigEndian(_w, ptr); ptr += 4;
	qToBigEndian(_h, ptr); ptr += 4;
	memcpy(ptr, _image.constData(), _image.length());
	ptr += _image.length();
	return ptr-data;
}

bool PutImage::payloadEquals(const Message &m) const
{
	const PutImage &p = static_cast<const PutImage&>(m);
	return
		layer() == p.layer() &&
		blendmode() == p.blendmode() &&
		x() == p.x() &&
		y() == p.y() &&
		width() == p.width() &&
		height() == p.height() &&
		image() == p.image();
}

FillRect *FillRect::deserialize(const uchar *data, uint len)
{
	if(len != 24)
		return 0;

	return new FillRect(
		*(data+0),
		qFromBigEndian<quint16>(data+1),
		*(data+3),
		qFromBigEndian<quint32>(data+4),
		qFromBigEndian<quint32>(data+8),
		qFromBigEndian<quint32>(data+12),
		qFromBigEndian<quint32>(data+16),
		qFromBigEndian<quint32>(data+20)
	);
}

int FillRect::payloadLength() const
{
	return 1 + 3 + 4*4 + 4;
}

int FillRect::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	qToBigEndian(_layer, ptr); ptr += 2;
	*(ptr++) = _blend;
	qToBigEndian(_x, ptr); ptr += 4;
	qToBigEndian(_y, ptr); ptr += 4;
	qToBigEndian(_w, ptr); ptr += 4;
	qToBigEndian(_h, ptr); ptr += 4;
	qToBigEndian(_color, ptr); ptr += 4;

	return ptr-data;
}


}
