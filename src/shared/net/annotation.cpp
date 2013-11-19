/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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
#include "annotation.h"

namespace protocol {

int AnnotationCreate::payloadLength() const
{
	return 1 + 4*2;
}

int AnnotationReshape::payloadLength() const
{
	return 1 + 4*2;
}

int AnnotationEdit::payloadLength() const
{
	return 1 + 4 + _text.length();
}

int AnnotationDelete::payloadLength() const
{
	return 1;
}

}
