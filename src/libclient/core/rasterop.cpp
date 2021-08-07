/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2018 Calle Laakkonen

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

#include "rasterop.h"

#include <QRgb>

namespace paintcore {

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

inline uint blend_blend(uchar base, uchar blend) {
	Q_UNUSED(base);
	return blend;
}

//! Multiply color values
inline uint blend_multiply(uchar base, uchar blend) {
	return qMin(255u, UINT8_MULT(base, blend));
}

//! Screen blend, the counterpart to multiply
inline uint blend_screen(uchar base, uchar blend) {
	return 255u - qMin(255u, UINT8_MULT(255u - base, 255u - blend));
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
			if(*mask==0) {
				// Special case: mask pixel is completely transparent
				dest += 4;
			}
			else if(*mask==255) {
				// Special case: mask pixel is completely opaque
				*dest = src[0]; ++dest;
				*dest = src[1]; ++dest;
				*dest = src[2]; ++dest;
				*dest = 255; ++dest;
			}
			else {
				// The usual case: blending required
				const uint srca1 = 255 - *mask;
				*dest = UINT8_MULT(src[0], *mask) + UINT8_MULT(*dest, srca1); ++dest;
				*dest = UINT8_MULT(src[1], *mask) + UINT8_MULT(*dest, srca1); ++dest;
				*dest = UINT8_MULT(src[2], *mask) + UINT8_MULT(*dest, srca1); ++dest;
				*dest = *mask +	UINT8_MULT(*dest, srca1); ++dest;
			}
		}
		dest += baseskip;
		mask += maskskip;
	}
}

void doAlphaMaskUnder(quint32 *base, quint32 color, const uchar *mask,
		int w, int h, int maskskip, int baseskip)
{
	baseskip *= 4;
	const uchar *src = reinterpret_cast<const uchar*>(&color);
	uchar *dest = reinterpret_cast<uchar*>(base);
	for(int y=0;y<h;++y) {
		for(int x=0;x<w;++x,++mask) {
			if((*mask==0) | (dest[3]==255)) {
				// Special case: transparent mask pixel or opaque destination pixel
				dest += 4;
			}
			else {
				// The usual case: blending required
				const uint a = UINT8_MULT(255-dest[3], *mask);
				*dest = UINT8_MULT(src[0], a) + *dest; ++dest;
				*dest = UINT8_MULT(src[1], a) + *dest; ++dest;
				*dest = UINT8_MULT(src[2], a) + *dest; ++dest;
				*dest = *dest + a; ++dest;
			}
		}
		dest += baseskip;
		mask += maskskip;
	}
}

struct fRGBA {
	qreal r, g, b, a;

	fRGBA() = default;
	fRGBA(quint32 pixel)
		: r(qRed(pixel) / 255.0),
		  g(qGreen(pixel) / 255.0),
		  b(qBlue(pixel) / 255.0),
		  a(qAlpha(pixel) / 255.0)
	{ }
	quint32 toPixel() const {
		return qRgba(r*255, g*255, b*255, a*255);
	}
};

// This was taken directly from GIMP's paint_funcs_color_erase_helper
void color_erase_helper(fRGBA *src, const fRGBA *color)
{
	fRGBA alpha;

	alpha.a = src->a;

	if (color->r < 0.0001)
		alpha.r = src->r;
	else if ( src->r > color->r )
		alpha.r = (src->r - color->r) / (1.0 - color->r);
	else if (src->r < color->r)
		alpha.r = (color->r - src->r) / color->r;
	else
		alpha.r = 0.0;

	if (color->g < 0.0001)
		alpha.g = src->g;
	else if ( src->g > color->g )
		alpha.g = (src->g - color->g) / (1.0 - color->g);
	else if ( src->g < color->g )
		alpha.g = (color->g - src->g) / (color->g);
	else
		alpha.g = 0.0;

	if (color->b < 0.0001)
		alpha.b = src->b;
	else if ( src->b > color->b )
		alpha.b = (src->b - color->b) / (1.0 - color->b);
	else if ( src->b < color->b )
		alpha.b = (color->b - src->b) / (color->b);
	else
		alpha.b = 0.0;

	if ( alpha.r > alpha.g ) {
		if ( alpha.r > alpha.b )
			src->a = alpha.r;
		else
			src->a = alpha.b;

	} else if ( alpha.g > alpha.b ) {
		src->a = alpha.g;

	} else {
		src->a = alpha.b;
	}

	src->a = (1.0 - color->a) + (src->a * color->a);

	if (src->a < 0.0001)
		return;

	src->r = (src->r - color->r) / src->a + color->r;
	src->g = (src->g - color->g) / src->a + color->g;
	src->b = (src->b - color->b) / src->a + color->b;

	src->a *= alpha.a;
}

// Specialized pixel composition: erase alpha channel
void doMaskErase(quint32 *base, const uchar *mask, int w, int h, int maskskip, int baseskip)
{
	baseskip *= 4;
	uchar *dest = reinterpret_cast<uchar*>(base);
	for(int y=0;y<h;++y) {
		for(int x=0;x<w;++x) {
			if((*mask==0) | (dest[3]==0)) {
				// Special case: fully transparent mask or destination
				dest += 4;
			}
			else {
				const uint a = 255 - *mask;
				*dest = UINT8_MULT(*dest, a); ++dest;
				*dest = UINT8_MULT(*dest, a); ++dest;
				*dest = UINT8_MULT(*dest, a); ++dest;
				*dest = UINT8_MULT(*dest, a); ++dest;
			}
			++mask;
		}
		dest += baseskip;
		mask += maskskip;
	}
}

void doMaskColorErase(quint32 *base, quint32 color, const uchar *mask, int w, int h, int maskskip, int baseskip)
{
	fRGBA col = color;

	for(int y=0;y<h;++y) {
		for(int x=0;x<w;++x) {
			if(*mask>0) {
				fRGBA src = qUnpremultiply(*base);
				col.a = *mask / 255.0;
				color_erase_helper(&src, &col);
				*base = qPremultiply(src.toPixel());
			}
			++mask;
			++base;
		}
		base += baseskip;
		mask += maskskip;
	}
}

// Specialized pixel composition: copy source without any blending
void doMaskCopy(quint32 *base, quint32 color, const uchar *mask, int w, int h, int maskskip, int baseskip)
{
	baseskip *= 4;
	uchar *dest = reinterpret_cast<uchar*>(base);
	const uchar *src = reinterpret_cast<const uchar*>(&color);
	for(int y=0;y<h;++y) {
		for(int x=0;x<w;++x) {
			*(dest++) = UINT8_MULT(src[0], *mask);
			*(dest++) = UINT8_MULT(src[1], *mask);
			*(dest++) = UINT8_MULT(src[2], *mask);
			*(dest++) = UINT8_MULT(src[3], *mask);
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
	const uchar *src = reinterpret_cast<const uchar*>(&color);
	for(int y=0;y<h;++y) {
		for(int x=0;x<w;++x,++mask) {
			if((*mask==0) | (*base == 0)) {
				// Special case: mask pixel or destination is completely transparent
				++base;

			} else {
				quint32 dest = qUnpremultiply(*reinterpret_cast<QRgb*>(base));
				uchar *d = reinterpret_cast<uchar*>(&dest);
				d[0] = UINT8_BLEND(BO(d[0], src[0]), d[0], *mask);
				d[1] = UINT8_BLEND(BO(d[1], src[1]), d[1], *mask);
				d[2] = UINT8_BLEND(BO(d[2], src[2]), d[2], *mask);
				*(base++) = qPremultiply(dest);
			}
		}
		base += baseskip;
		mask += maskskip;
	}
}

std::array<quint32, 5> sampleMask(const quint32 *pixels, const uchar *mask, int w, int h, int maskskip, int pixelskip)
{
	std::array<quint32, 5> result{ {0, 0, 0, 0, 0} };
	pixelskip *= 4;
	const uchar *pix = reinterpret_cast<const uchar*>(pixels);
	for(int y=0;y<h;++y) {
		for(int x=0;x<w;++x,++mask) {
			const uchar m = *mask;
			const uchar a = pix[3];
			result[0] += m;

			result[1] += UINT8_MULT(pix[2], m); // red
			result[2] += UINT8_MULT(pix[1], m); // green
			result[3] += UINT8_MULT(pix[0], m); // blue
			result[4] += UINT8_MULT(a, m); // alpha
			pix += 4;
		}
		pix += pixelskip;
		mask += maskskip;
	}

	return result;
}

void doPixelAlphaBlend(quint32 *destination, const quint32 *source, uchar opacity, int len)
{
	uchar *dest = reinterpret_cast<uchar*>(destination);
	const uchar *src = reinterpret_cast<const uchar*>(source);

	while(len--) {
		const uint asrc = 255 - UINT8_MULT(src[3], opacity);
		if(asrc==255) {
			src += 4;
			dest += 4;
		} else {
			*dest = UINT8_MULT(*src, opacity) + UINT8_MULT(*dest, asrc); ++dest, ++src;
			*dest = UINT8_MULT(*src, opacity) + UINT8_MULT(*dest, asrc); ++dest, ++src;
			*dest = UINT8_MULT(*src, opacity) + UINT8_MULT(*dest, asrc); ++dest, ++src;
			*dest = UINT8_MULT(*src, opacity) + UINT8_MULT(*dest, asrc); ++dest, ++src;
		}
	}
}

void doPixelAlphaUnder(quint32 *destination, const quint32 *source, uchar opacity, int len)
{
	uchar *dest = reinterpret_cast<uchar*>(destination);
	const uchar *src = reinterpret_cast<const uchar*>(source);

	while(len--) {
		if((src[3]==0) | (dest[3]==255)) {
			// Special case: transparent source pixel or opaque destination pixel
			dest += 4;
			src += 4;
		}
		else {
			// The usual case: blending required
			const uint asrc = UINT8_MULT(255 - dest[3], UINT8_MULT(src[3], opacity));
			*dest = UINT8_MULT(*src, asrc) + (*dest); ++dest, ++src;
			*dest = UINT8_MULT(*src, asrc) + (*dest); ++dest, ++src;
			*dest = UINT8_MULT(*src, asrc) + (*dest); ++dest, ++src;
			*dest = UINT8_MULT(*src, asrc) + (*dest); ++dest, ++src;
		}
	}
}

// Specialized pixel composition: erase alpha channel
void doPixelErase(quint32 *destination, const quint32 *source, uchar opacity, int len)
{
	uchar *dest = reinterpret_cast<uchar*>(destination);
	const uchar *src = reinterpret_cast<const uchar*>(source)+3;
	while(len--) {
		const uint a = 255 - UINT8_MULT(*src, opacity);
		*dest = UINT8_MULT(*dest, a); ++dest;
		*dest = UINT8_MULT(*dest, a); ++dest;
		*dest = UINT8_MULT(*dest, a); ++dest;
		*dest = UINT8_MULT(*dest, a); ++dest;
		src += 4;
	}
}

void doPixelColorErase(quint32 *destination, const quint32 *source, uchar opacity, int len)
{
	const qreal o = opacity / 255.0;

	while(len--) {
		fRGBA d = qUnpremultiply(*destination);
		fRGBA s = qUnpremultiply(*source);
		s.a *= o;
		color_erase_helper(&d, &s);
		*destination = qPremultiply(d.toPixel());
		++destination;
		++source;
	}
}

void tintPixels(quint32 *pixels, int len, quint32 tint)
{
	const uchar *t = reinterpret_cast<uchar*>(&tint);

	const uint a0 = t[3];
	const uint a1 = 255 - a0;

	while(len--) {
		quint32 c = qUnpremultiply(*pixels);
		uchar *cc = reinterpret_cast<uchar*>(&c);
		cc[0] = UINT8_MULT(a1, cc[0]) + UINT8_MULT(a0, t[0]);
		cc[1] = UINT8_MULT(a1, cc[1]) + UINT8_MULT(a0, t[1]);
		cc[2] = UINT8_MULT(a1, cc[2]) + UINT8_MULT(a0, t[2]);
		*(pixels++) = qPremultiply(c);
	}
}

template<BlendOp BO>
void doPixelComposite(quint32 *base, const quint32 *source, uchar alpha, int len)
{
	while(len--) {
		if(*source & *base) {
			// Blend only if neither source nor destination pixels are fully transparent
			quint32 dest = qUnpremultiply(*reinterpret_cast<QRgb*>(base));
			const quint32 src = qUnpremultiply(*reinterpret_cast<const QRgb*>(source));

			auto *d = reinterpret_cast<uchar*>(&dest);
			auto *s = reinterpret_cast<const uchar*>(&src);

			const uchar a = UINT8_MULT(s[3], alpha);

			d[0] = UINT8_BLEND(BO(d[0], s[0]), d[0], a);
			d[1] = UINT8_BLEND(BO(d[1], s[1]), d[1], a);
			d[2] = UINT8_BLEND(BO(d[2], s[2]), d[2], a);

			*base = qPremultiply(dest);
		}
		++base;
		++source;
	}
}

void compositeMask(BlendMode::Mode mode, quint32 *base, quint32 color, const uchar *mask,
		int w, int h, int maskskip, int baseskip)
{
	switch(mode) {
	case BlendMode::MODE_ERASE: doMaskErase(base, mask, w, h, maskskip, baseskip); break;
	case BlendMode::MODE_NORMAL: doAlphaMaskBlend(base, color, mask, w, h, maskskip, baseskip); break;
	case BlendMode::MODE_MULTIPLY: doMaskComposite<blend_multiply>(base, color, mask, w, h, maskskip, baseskip); break;
	case BlendMode::MODE_SCREEN: doMaskComposite<blend_screen>(base, color, mask, w, h, maskskip, baseskip); break;
	case BlendMode::MODE_DIVIDE: doMaskComposite<blend_divide>(base, color, mask, w, h, maskskip, baseskip); break;
	case BlendMode::MODE_BURN: doMaskComposite<blend_burn>(base, color, mask, w, h, maskskip, baseskip); break;
	case BlendMode::MODE_DODGE: doMaskComposite<blend_dodge>(base, color, mask, w, h, maskskip, baseskip); break;
	case BlendMode::MODE_DARKEN: doMaskComposite<blend_darken>(base, color, mask, w, h, maskskip, baseskip); break;
	case BlendMode::MODE_LIGHTEN: doMaskComposite<blend_lighten>(base, color, mask, w, h, maskskip, baseskip); break;
	case BlendMode::MODE_SUBTRACT: doMaskComposite<blend_subtract>(base, color, mask, w, h, maskskip, baseskip); break;
	case BlendMode::MODE_ADD: doMaskComposite<blend_add>(base, color, mask, w, h, maskskip, baseskip); break;
	case BlendMode::MODE_RECOLOR: doMaskComposite<blend_blend>(base, color, mask, w, h, maskskip, baseskip); break;
	case BlendMode::MODE_BEHIND: doAlphaMaskUnder(base, color, mask, w, h, maskskip, baseskip); break;
	case BlendMode::MODE_COLORERASE: doMaskColorErase(base, color, mask, w, h, maskskip, baseskip); break;
	case BlendMode::MODE_REPLACE: doMaskCopy(base, color, mask, w, h, maskskip, baseskip); break;
	}
}

void compositePixels(BlendMode::Mode mode, quint32 *base, const quint32 *over, int len, uchar opacity)
{
	Q_ASSERT(len>=0);

	switch(mode) {
	case BlendMode::MODE_ERASE: doPixelErase(base, over, opacity, len); break;
	case BlendMode::MODE_NORMAL: doPixelAlphaBlend(base, over, opacity, len); break;
	case BlendMode::MODE_MULTIPLY: doPixelComposite<blend_multiply>(base, over, opacity, len); break;
	case BlendMode::MODE_SCREEN: doPixelComposite<blend_screen>(base, over, opacity, len); break;
	case BlendMode::MODE_DIVIDE: doPixelComposite<blend_divide>(base, over, opacity, len); break;
	case BlendMode::MODE_BURN: doPixelComposite<blend_burn>(base, over, opacity, len); break;
	case BlendMode::MODE_DODGE: doPixelComposite<blend_dodge>(base, over, opacity, len); break;
	case BlendMode::MODE_DARKEN: doPixelComposite<blend_darken>(base, over, opacity, len); break;
	case BlendMode::MODE_LIGHTEN: doPixelComposite<blend_lighten>(base, over, opacity, len); break;
	case BlendMode::MODE_SUBTRACT: doPixelComposite<blend_subtract>(base, over, opacity, len); break;
	case BlendMode::MODE_ADD: doPixelComposite<blend_add>(base, over, opacity, len); break;
	case BlendMode::MODE_RECOLOR: doPixelComposite<blend_blend>(base, over, opacity, len); break;
	case BlendMode::MODE_BEHIND: doPixelAlphaUnder(base, over, opacity, len); break;
	case BlendMode::MODE_COLORERASE: doPixelColorErase(base, over, opacity, len); break;
	case BlendMode::MODE_REPLACE: /* not implemented */ break;
	}
}

}
