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

#include "rasterop.h"

namespace dpcore {

const char *BLEND_MODE[BLEND_MODES] = {
	QT_TR_NOOP("Normal"),
	QT_TR_NOOP("Multiply"),
	//QT_TR_NOOP("Lighten"),
	//QT_TR_NOOP("Darken"),
};

// This is borrowed from Pigment of koffice libs:
/// Blending of two scale values as described by the alpha scale value
/// A scale value is interpreted as 255 equaling 1.0 (such as seen in rgb8 triplets)
/// Basically we do: a*alpha + b*(1-alpha)
inline uint UINT8_BLEND(uint a, uint b, uint alpha)
{
    // However the formula is refactored to (a-b)*alpha + b  since that saves a multiplication
    // Signed arithmetic is needed since a-b might be negative
    // +b above becomes + (b<<8) - b  because we multiply it with 255 to fit the first part
    //  That way we can do a normal rounding
    uint c = uint(((int(a) - int(b)) * int(alpha)) + (b<<8) - b) + 0x80u;

    return ((c >> 8) + c) >> 8;
}

/// multiplication of two scale values
/// A scale value is interpreted as 255 equaling 1.0 (such as seen in rgb8 triplets)
/// thus "255*255=255" because 1.0*1.0=1.0
inline uint UINT8_MULT(uint a, uint b)
{
    uint c = a * b + 0x80u;
    return ((c >> 8) + c) >> 8;
}

inline uint UINT8_DIVIDE(uint a, uint b)
{
    uint c = (a * 255u + (b / 2u)) / b;
    return c;
}

//! Regular alpha blender
/**
 * base = base * (1-opacity) + blend * opacity
 */
inline void blend_normal(uchar *base, const uchar *blend, int opacity) {
	base[0] = UINT8_BLEND(blend[0], base[0], opacity);
	base[1] = UINT8_BLEND(blend[1], base[1], opacity);
	base[2] = UINT8_BLEND(blend[2], base[2], opacity);
	// TODO: blend alpha too once we need it
}

//! Multiply color values
inline void blend_multiply(uchar *base, const uchar *blend, int opacity) {
	uint r = qMin(255u, UINT8_MULT(base[0], blend[0]));
	uint g = qMin(255u, UINT8_MULT(base[1], blend[1]));
	uint b = qMin(255u, UINT8_MULT(base[2], blend[2]));
	base[0] = UINT8_BLEND(r, base[0], opacity);
	base[1] = UINT8_BLEND(g, base[1], opacity);
	base[2] = UINT8_BLEND(b, base[2], opacity);
}

void compositeMask(int mode, quint32 *base, quint32 color, const uchar *mask,
		int w, int h, int maskskip, int baseskip)
{
	for(int y=0;y<h;++y) {
		for(int x=0;x<w;++x) {
			// Move the switch out of the loop
			switch(mode) {
				case 1:
				blend_multiply(reinterpret_cast<uchar*>(base), reinterpret_cast<uchar*>(&color), *mask);
				break;
			default:
				blend_normal(reinterpret_cast<uchar*>(base), reinterpret_cast<uchar*>(&color), *mask);
				break;
			}
			++base;
			++mask;
		}
		base += baseskip;
		mask += maskskip;
	}
}

void compositePixels(int mode, quint32 *base, quint32 *over, int len)
{
	while(len--) {
		blend_normal(reinterpret_cast<uchar*>(base),
					reinterpret_cast<const uchar*>(over), *over>>24);
		++base;
		++over;
	}

}

}

