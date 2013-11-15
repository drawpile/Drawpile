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

#include "layer.h"

namespace protocol {

int LayerCreate::payloadLength() const
{
	return sizeof(LayerId) + 3;
}

int LayerAttributes::payloadLength() const
{
	return sizeof(LayerId) + 2 + _title.length();
}

int LayerOrder::payloadLength() const
{
	return _order.size() * sizeof(LayerId);
}

int LayerDelete::payloadLength() const
{
	return sizeof(LayerId);
}


}
