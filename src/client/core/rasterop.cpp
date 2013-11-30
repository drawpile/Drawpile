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

#include "rasterop.h"

namespace dpcore {

const char *BLEND_MODE[BLEND_MODES] = {
	QT_TR_NOOP("Erase"), // This is a special mode
	QT_TR_NOOP("Normal"),
	QT_TR_NOOP("Multiply"),
	QT_TR_NOOP("Divide"),
	QT_TR_NOOP("Burn"),
	QT_TR_NOOP("Dodge"),
	QT_TR_NOOP("Darken"),
	QT_TR_NOOP("Lighten"),
	QT_TR_NOOP("Subtract"),
	QT_TR_NOOP("Add"),
};

static const QString svgModeNames[BLEND_MODES] = {
	QString("-dp-erase"), /* this is used internally only */
	QString("src-over"),
	QString("multiply"),
	QString("screen"),
	QString("color-burn"),
	QString("color-dodge"),
	QString("darken"),
	QString("lighten"),
	QString("-dp-minus"), /* not part of SVG or OpenRaster spec */
	QString("plus")
};

int blendModeSvg(const QString &name)
{
	QStringRef n;
	if(name.startsWith("svg:"))
		n = name.midRef(4);
	else
		n = name.midRef(0);

	for(int i=0;i<BLEND_MODES;++i)
		if(svgModeNames[i] == n)
			return i;
	return -1;
}

const QString &svgBlendMode(int blendmode)
{
	if(blendmode < 0 || blendmode>=BLEND_MODES)
		return svgModeNames[1];
	return svgModeNames[blendmode];
}

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

// Normal alpha blend
void doAlphaMaskBlend(quint32 *base, quint32 color, const uchar *mask,
		int w, int h, int maskskip, int baseskip)
{
	baseskip *= 4;
	const uchar *src = reinterpret_cast<const uchar*>(&color);
	uchar *dest = reinterpret_cast<uchar*>(base);
	for(int y=0;y<h;++y) {
		for(int x=0;x<w;++x,++mask) {
			// Special case: mask pixel is completely transparent
			if(*mask==0) {
				dest += 4;
			}
			// Special case: mask pixel is completely opaque
			else if(*mask==255) {
				*dest = src[0]; ++dest;
				*dest = src[1]; ++dest;
				*dest = src[2]; ++dest;
				*dest = 255; ++dest;
			}
			// The usual case: blending required
			else {
				if(dest[3]==0) {
					// Special case: target is completely transparent, we can overwrite it.
					*dest = src[0]; ++dest;
					*dest = src[1]; ++dest;
					*dest = src[2]; ++dest;
					*dest = *mask; ++dest;
				} else {
					// Normal case: blend colors and alpha
					*dest = UINT8_BLEND(src[0], *dest, *mask); ++dest;
					*dest = UINT8_BLEND(src[1], *dest, *mask); ++dest;
					*dest = UINT8_BLEND(src[2], *dest, *mask); ++dest;
					*dest = *mask + UINT8_MULT(255-*mask, *dest); ++dest;
				}
			}
		}
		dest += baseskip;
		mask += maskskip;
	}
}

// Specialized pixel composition: erase alpha channel
void doMaskErase(quint32 *base, const uchar *mask, int w, int h, int maskskip, int baseskip)
{
	baseskip *= 4;
	uchar *dest = reinterpret_cast<uchar*>(base) + 3;
	for(int y=0;y<h;++y) {
		for(int x=0;x<w;++x) {
			*dest = qMax(0, *dest - *mask);

			dest += 4;
			++mask;
		}
		dest += baseskip;
		mask += maskskip;
	}
}

// A generic composition function for special blending modes
// This doesn't touch the alpha channel.
typedef uint(*BlendOp)(uchar,uchar);
template<BlendOp BO>
void doMaskComposite(quint32 *base, quint32 color, const uchar *mask,
		int w, int h, int maskskip, int baseskip)
{
	baseskip *= 4;
	const uchar *src = reinterpret_cast<const uchar*>(&color);
	uchar *dest = reinterpret_cast<uchar*>(base);
	for(int y=0;y<h;++y) {
		for(int x=0;x<w;++x,++mask) {
			// Special case: mask pixel is completely transparent
			if(*mask==0) {
				dest += 4;
			}
			// Special case: mask pixel is completely opaque
			else if(*mask==255) {
				*dest = BO(*dest, src[0]); ++dest;
				*dest = BO(*dest, src[1]); ++dest;
				*dest = BO(*dest, src[2]); ++dest;
				++dest;
			}
			// The usual case: blending required
			else {
				if(dest[3]>0) {
					*dest = UINT8_BLEND(BO(*dest, src[0]), *dest, *mask); ++dest;
					*dest = UINT8_BLEND(BO(*dest, src[1]), *dest, *mask); ++dest;
					*dest = UINT8_BLEND(BO(*dest, src[2]), *dest, *mask); ++dest;
					++dest;
				} else {
					// No need to do anything if destination pixel is fully transparent
					dest += 4;
				}
			}
		}
		dest += baseskip;
		mask += maskskip;
	}
}

void doPixelAlphaBlend(quint32 *destination, const quint32 *source, uchar opacity, int len)
{
	uchar *dest = reinterpret_cast<uchar*>(destination);
	const uchar *src = reinterpret_cast<const uchar*>(source);
	while(len--) {
		const uchar a = UINT8_MULT(src[3], opacity);
		const uchar a2 = UINT8_MULT(255-a, dest[3]);
		*dest = UINT8_MULT(a, src[0]) + UINT8_MULT(a2, *dest); ++dest;
		*dest = UINT8_MULT(a, src[1]) + UINT8_MULT(a2, *dest); ++dest;
		*dest = UINT8_MULT(a, src[2]) + UINT8_MULT(a2, *dest); ++dest;
		*dest = a + a2; ++dest;
		src+=4;
	}
}

// Specialized pixel composition: erase alpha channel
void doPixelErase(quint32 *destination, const quint32 *source, uchar opacity, int len)
{
	uchar *dest = reinterpret_cast<uchar*>(destination) + 3;
	const uchar *src = reinterpret_cast<const uchar*>(source) + 3;
	while(len--) {
		uchar a = qMax(0, *dest - int(UINT8_MULT(*src, opacity)));
		*dest = a;
		dest += 4;
		src += 4;
	}
}


template<BlendOp BO>
void doPixelComposite(quint32 *destination, const quint32 *source, uchar alpha, int len)
{
	const uchar *src = reinterpret_cast<const uchar*>(source);
	uchar *dest = reinterpret_cast<uchar*>(destination);
	while(len--) {
		// Special case: source pixel is completely transparent
		if(*src==0) {
			dest += 4;
		}
		// Special case: source pixel is completely opaque
		else if(*src==255) {
			*dest = BO(*dest, src[0]); ++dest;
			*dest = BO(*dest, src[1]); ++dest;
			*dest = BO(*dest, src[2]); ++dest;
			++dest;
		}
		// The usual case: blending required
		else {
			if(dest[3]>0) {
				const uchar a = UINT8_MULT(src[3], alpha);
				const uchar a2 = UINT8_MULT(a, dest[3]);
				*dest = UINT8_BLEND(BO(*dest, src[0]), *dest, a2); ++dest;
				*dest = UINT8_BLEND(BO(*dest, src[1]), *dest, a2); ++dest;
				*dest = UINT8_BLEND(BO(*dest, src[2]), *dest, a2); ++dest;
				++dest;
			} else {
				// No need to do anything if destination pixel is fully transparent
				dest += 4;
			}
		}
		src += 4;
	}
}

void compositeMask(int mode, quint32 *base, quint32 color, const uchar *mask,
		int w, int h, int maskskip, int baseskip)
{
	// Note! Make sure the these are in the correct order!
	switch(mode) {
	case 0: doMaskErase(base, mask, w, h, maskskip, baseskip); break;
	case 1: doAlphaMaskBlend(base, color, mask, w, h, maskskip, baseskip); break;
	case 2: doMaskComposite<blend_multiply>(base, color, mask, w, h, maskskip, baseskip); break;
	case 3: doMaskComposite<blend_divide>(base, color, mask, w, h, maskskip, baseskip); break;
	case 4: doMaskComposite<blend_burn>(base, color, mask, w, h, maskskip, baseskip); break;
	case 5: doMaskComposite<blend_dodge>(base, color, mask, w, h, maskskip, baseskip); break;
	case 6: doMaskComposite<blend_darken>(base, color, mask, w, h, maskskip, baseskip); break;
	case 7: doMaskComposite<blend_lighten>(base, color, mask, w, h, maskskip, baseskip); break;
	case 8: doMaskComposite<blend_subtract>(base, color, mask, w, h, maskskip, baseskip); break;
	case 9: doMaskComposite<blend_add>(base, color, mask, w, h, maskskip, baseskip); break;
	}
}

void compositePixels(int mode, quint32 *base, const quint32 *over, int len, uchar opacity)
{
	// Note! Make sure the these are in the correct order!
	switch(mode) {
	case 0: doPixelErase(base, over, opacity, len); break;
	case 1: doPixelAlphaBlend(base, over, opacity, len); break;
	case 2: doPixelComposite<blend_multiply>(base, over, opacity, len); break;
	case 3: doPixelComposite<blend_divide>(base, over, opacity, len); break;
	case 4: doPixelComposite<blend_burn>(base, over, opacity, len); break;
	case 5: doPixelComposite<blend_dodge>(base, over, opacity, len); break;
	case 6: doPixelComposite<blend_darken>(base, over, opacity, len); break;
	case 7: doPixelComposite<blend_lighten>(base, over, opacity, len); break;
	case 8: doPixelComposite<blend_subtract>(base, over, opacity, len); break;
	case 9: doPixelComposite<blend_add>(base, over, opacity, len); break;
	}
}

}
