/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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
#include "libshared/net/undo.h"

namespace protocol {

Undo *Undo::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=2)
		return 0;
	return new Undo(
		ctx,
		data[0],
		data[1]
	);
}

int Undo::payloadLength() const
{
	return 2;
}

int Undo::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = m_override;
	*(ptr++) = m_redo;
	return ptr-data;
}

bool Undo::payloadEquals(const Message &m) const
{
	const Undo &u = static_cast<const Undo&>(m);
	return
		overrideId() == u.overrideId() &&
		isRedo() == u.isRedo();
}

Kwargs Undo::kwargs() const
{
	Kwargs kw;
	if(overrideId()>0)
		kw["override"] = QString::number(overrideId());
	return kw;
}

Undo *Undo::fromText(uint8_t ctx, const Kwargs &kwargs, bool redo)
{
	return new Undo(
		ctx,
		kwargs["override"].toInt(),
		redo
		);
}

}
