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
	QT_TR_NOOP("Divide"),
	QT_TR_NOOP("Dodge"),
	QT_TR_NOOP("Burn"),
	QT_TR_NOOP("Darken"),
	QT_TR_NOOP("Lighten"),
	QT_TR_NOOP("Add"),
	QT_TR_NOOP("Subtract"),
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
inline void blend_normal(uchar *base, const uchar *blend, uint opacity) {
	base[0] = UINT8_BLEND(blend[0], base[0], opacity);
	base[1] = UINT8_BLEND(blend[1], base[1], opacity);
	base[2] = UINT8_BLEND(blend[2], base[2], opacity);
	// TODO: blend alpha too once we need it
}

//! Multiply color values
inline void blend_multiply(uchar *base, const uchar *blend, uint opacity) {
	const uint r = qMin(255u, UINT8_MULT(base[0], blend[0]));
	const uint g = qMin(255u, UINT8_MULT(base[1], blend[1]));
	const uint b = qMin(255u, UINT8_MULT(base[2], blend[2]));
	base[0] = UINT8_BLEND(r, base[0], opacity);
	base[1] = UINT8_BLEND(g, base[1], opacity);
	base[2] = UINT8_BLEND(b, base[2], opacity);
}

//! Divide color values
inline void blend_divide(uchar *base, const uchar *blend, uint opacity) {
	const uint r = qMin(255u, (base[0]*256u + blend[0]/2) / (1+blend[0]));
	const uint g = qMin(255u, (base[1]*256u + blend[1]/2) / (1+blend[1]));
	const uint b = qMin(255u, (base[2]*256u + blend[2]/2) / (1+blend[2]));
	base[0] = UINT8_BLEND(r, base[0], opacity);
	base[1] = UINT8_BLEND(g, base[1], opacity);
	base[2] = UINT8_BLEND(b, base[2], opacity);
}

//! Darken color
inline void blend_darken(uchar *base, const uchar *blend, uint opacity) {
	const uint r = qMin(base[0], blend[0]);
	const uint g = qMin(base[1], blend[1]);
	const uint b = qMin(base[2], blend[2]);
	base[0] = UINT8_BLEND(r, base[0], opacity);
	base[1] = UINT8_BLEND(g, base[1], opacity);
	base[2] = UINT8_BLEND(b, base[2], opacity);
}

//! Lighten color
inline void blend_lighten(uchar *base, const uchar *blend, uint opacity) {
	const uint r = qMax(base[0], blend[0]);
	const uint g = qMax(base[1], blend[1]);
	const uint b = qMax(base[2], blend[2]);
	base[0] = UINT8_BLEND(r, base[0], opacity);
	base[1] = UINT8_BLEND(g, base[1], opacity);
	base[2] = UINT8_BLEND(b, base[2], opacity);
}

//! Color dodge
inline void blend_dodge(uchar *base, const uchar *blend, uint opacity) {
	const uint r = qMin(255u, base[0] * 256u / (254u - blend[0]));
	const uint g = qMin(255u, base[1] * 256u / (254u - blend[1]));
	const uint b = qMin(255u, base[2] * 256u / (254u - blend[2]));
	base[0] = UINT8_BLEND(r, base[0], opacity);
	base[1] = UINT8_BLEND(g, base[1], opacity);
	base[2] = UINT8_BLEND(b, base[2], opacity);
}

//! Color burn
inline void blend_burn(uchar *base, const uchar *blend, uint opacity) {
	const uint r = qBound(0, (255 - ((255-base[0])*256 / (blend[0]+1))), 255);
	const uint g = qBound(0, (255 - ((255-base[1])*256 / (blend[1]+1))), 255);
	const uint b = qBound(0, (255 - ((255-base[2])*256 / (blend[2]+1))), 255);

	base[0] = UINT8_BLEND(r, base[0], opacity);
	base[1] = UINT8_BLEND(g, base[1], opacity);
	base[2] = UINT8_BLEND(b, base[2], opacity);
}

//! Add colors
inline void blend_add(uchar *base, const uchar *blend, uint opacity) {
	const uint r = qMin(base[0]+blend[0], 255);
	const uint g = qMin(base[1]+blend[1], 255);
	const uint b = qMin(base[2]+blend[2], 255);

	base[0] = UINT8_BLEND(r, base[0], opacity);
	base[1] = UINT8_BLEND(g, base[1], opacity);
	base[2] = UINT8_BLEND(b, base[2], opacity);
}

//! Subtract colors
inline void blend_subtract(uchar *base, const uchar *blend, uint opacity) {
	const uint r = qMax(base[0]-blend[0], 0);
	const uint g = qMax(base[1]-blend[1], 0);
	const uint b = qMax(base[2]-blend[2], 0);

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
			// Note! Make sure the these are in the correct order!
			switch(mode) {
				case 1:
				blend_multiply(reinterpret_cast<uchar*>(base), reinterpret_cast<uchar*>(&color), *mask);
				break;
				case 2:
				blend_divide(reinterpret_cast<uchar*>(base), reinterpret_cast<uchar*>(&color), *mask);
				break;
				case 3:
				blend_dodge(reinterpret_cast<uchar*>(base), reinterpret_cast<uchar*>(&color), *mask);
				break;
				case 4:
				blend_burn(reinterpret_cast<uchar*>(base), reinterpret_cast<uchar*>(&color), *mask);
				break;
				case 5:
				blend_darken(reinterpret_cast<uchar*>(base), reinterpret_cast<uchar*>(&color), *mask);
				break;
				case 6:
				blend_lighten(reinterpret_cast<uchar*>(base), reinterpret_cast<uchar*>(&color), *mask);
				break;
				case 7:
				blend_add(reinterpret_cast<uchar*>(base), reinterpret_cast<uchar*>(&color), *mask);
				break;
				case 8:
				blend_subtract(reinterpret_cast<uchar*>(base), reinterpret_cast<uchar*>(&color), *mask);
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

