/*
   DrawPile - a collaborative drawing program.

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
#include "undo.h"

namespace protocol {

UndoPoint *UndoPoint::deserialize(const uchar *data, uint len)
{
	if(len!=1)
		return 0;
	return new UndoPoint(*data);
}

int UndoPoint::payloadLength() const
{
	return 1;
}

int UndoPoint::serializePayload(uchar *data) const
{
	*data = contextId();
	return 1;
}


Undo *Undo::deserialize(const uchar *data, uint len)
{
	if(len!=3)
		return 0;
	return new Undo(
		data[0],
		data[1],
		data[2]
	);
}

int Undo::payloadLength() const
{
	return 1 + 2;
}

int Undo::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	*(ptr++) = _override;
	*(ptr++) = _points;
	return ptr-data;
}

}
