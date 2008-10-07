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
inline uint blend_normal(uchar base, uchar blend) {
	return blend;
}

//! Multiply color values
inline uint blend_multiply(uchar base, uchar blend) {
	return qMin(255u, UINT8_MULT(base, blend));
}

//! Divide color values
inline uint blend_divide(uchar base, uchar blend) {
	return qMin(255u, (base*256u + blend/2) / (1+blend));
}

//! Darken color
inline uint blend_darken(uchar base, uchar blend) {
	return qMin(base, blend);
}

//! Lighten color
inline uint blend_lighten(uchar base, uchar blend) {
	return qMax(base, blend);
}

//! Color dodge
inline uint blend_dodge(uchar base, uchar blend) {
	return qMin(255u, base * 256u / (256u - blend));
}

//! Color burn
inline uint blend_burn(uchar base, uchar blend) {
	return qBound(0, (255 - ((255-base)*256 / (blend+1))), 255);
}

//! Add colors
inline uint blend_add(uchar base, uchar blend) {
	return qMin(base+blend, 255);
}

//! Subtract colors
inline uint blend_subtract(uchar base, uchar blend) {
	return qMax(base-blend, 0);
}

typedef uint(*BlendOp)(uchar,uchar);
template<BlendOp BO>
void doComposite(quint32 *base, quint32 color, const uchar *mask,
		int w, int h, int maskskip, int baseskip)
{
	const uchar *src = reinterpret_cast<const uchar*>(&color);
	uchar *dest = reinterpret_cast<uchar*>(base);
	for(int y=0;y<h;++y) {
		for(int x=0;x<w;++x) {
			*dest = UINT8_BLEND(BO(*dest, src[0]), *dest, *mask); ++dest;
			*dest = UINT8_BLEND(BO(*dest, src[1]), *dest, *mask); ++dest;
			*dest = UINT8_BLEND(BO(*dest, src[2]), *dest, *mask); ++dest;
			++dest;
			++mask;
		}
		dest += baseskip*4;
		mask += maskskip;
	}
}

void compositeMask(int mode, quint32 *base, quint32 color, const uchar *mask,
		int w, int h, int maskskip, int baseskip)
{
		// Note! Make sure the these are in the correct order!
		switch(mode) {
			case 1:
				doComposite<blend_multiply>(base, color, mask, w, h, maskskip, baseskip);
				break;
			case 2:
				doComposite<blend_divide>(base, color, mask, w, h, maskskip, baseskip);
				break;
			case 3:
				doComposite<blend_dodge>(base, color, mask, w, h, maskskip, baseskip);
				break;
			case 4:
				doComposite<blend_burn>(base, color, mask, w, h, maskskip, baseskip);
				break;
			case 5:
				doComposite<blend_darken>(base, color, mask, w, h, maskskip, baseskip);
				break;
			case 6:
				doComposite<blend_lighten>(base, color, mask, w, h, maskskip, baseskip);
				break;
			case 7:
				doComposite<blend_add>(base, color, mask, w, h, maskskip, baseskip);
				break;
			case 8:
				doComposite<blend_subtract>(base, color, mask, w, h, maskskip, baseskip);
				break;
			default:
				doComposite<blend_normal>(base, color, mask, w, h, maskskip, baseskip);
				break;
		}
}

void compositePixels(int mode, quint32 *base, const quint32 *over, int len)
{
	uchar *dest = reinterpret_cast<uchar*>(base);
	const uchar *src = reinterpret_cast<const uchar*>(over);
	while(len--) {
		*dest = UINT8_BLEND(src[0], *dest, src[3]); ++dest;
		*dest = UINT8_BLEND(src[1], *dest, src[3]); ++dest;
		*dest = UINT8_BLEND(src[2], *dest, src[3]); ++dest;
		++dest;
		src+=4;
	}

}

}

