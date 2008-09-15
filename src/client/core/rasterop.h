/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

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
#ifndef RASTEROP_H
#define RASTEROP_H

namespace dpcore {

//! Regular alpha blender
/**
 * base = base * (1-opacity) + blend * opacity
 */
inline void blend_normal(uchar *base, const uchar *blend, int opacity) {
	base[0] = (base[0] * (255-opacity) / 255 + blend[0] * opacity / 255);
	base[1] = (base[1] * (255-opacity) / 255 + blend[1] * opacity / 255);
	base[2] = (base[2] * (255-opacity) / 255 + blend[2] * opacity / 255);
	//base[3] = (base[3] * (255-opacity) / 255 + blend[3] * opacity / 255);
}

}

#endif

