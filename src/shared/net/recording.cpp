/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include "recording.h"

namespace protocol {

Interval *Interval::deserialize(const uchar *data, uint len)
{
	if(len!=2)
		return 0;
	return new Interval(qFromBigEndian<quint16>(data));
}

int Interval::payloadLength() const
{
	return 2;
}

int Interval::serializePayload(uchar *data) const
{
	qToBigEndian(_msecs, data);
	return 2;
}

}

