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
#include "image.h"

namespace protocol {

PutImage *PutImage::deserialize(const uchar *data, uint len)
{
	if(len < 11)
		return 0;

	return new PutImage(
		*data,
		*(data+1),
		qFromBigEndian<quint16>(data+2),
		qFromBigEndian<quint16>(data+4),
		qFromBigEndian<quint16>(data+6),
		qFromBigEndian<quint16>(data+8),
		QByteArray((const char*)data+10, len-10)
	);
}

int PutImage::payloadLength() const
{
	return 1 + 1 + 4*2 + _image.size();
}

int PutImage::serializePayload(uchar *data) const
{
	*data = _layer; ++data;
	*data = _flags; ++data;
	qToBigEndian(_x, data); data += 2;
	qToBigEndian(_y, data); data += 2;
	qToBigEndian(_w, data); data += 2;
	qToBigEndian(_h, data); data += 2;
	memcpy(data, _image.constData(), _image.length());
	return 1 + 1 + 4*2 + _image.size();
}

}
