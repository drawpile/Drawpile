/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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
#ifndef RASTEROP_H
#define RASTEROP_H

#include <QString>
#include <array>

namespace paintcore {

struct BlendMode {
	enum Flag {
		PrivateMode = 0x00, // not available for selection
		LayerMode   = 0x01, // available for use as a layer mode
		BrushMode   = 0x02, // available for use as a brush mode
		UniversalMode = 0x03, // can be used with brushes and layers
		DecrOpacity = 0x04, // this mode can decrease pixel opacity
		IncrOpacity = 0x08  // this mode can increase pixel opacity
	};
	Q_DECLARE_FLAGS(Flags, Flag)

	const char *name;      // translatable name
	const QString svgname; // SVG style name of this blending mode
	const int id;          // ID as used in the protocol
	const Flags flags;     // Metadata

	BlendMode() : name(nullptr), id(-1) { }
	BlendMode(const char *n, const QString &s, int i, Flags f)
		: name(n), svgname(s), id(i), flags(f) { }
};

Q_DECLARE_OPERATORS_FOR_FLAGS(BlendMode::Flags)

// Note. Protocol ordering and display ordering of the modes differ.
static const int BLEND_MODES=13;
extern const BlendMode BLEND_MODE[BLEND_MODES];

/**
 * @brief Find the blending mode with the given protocol ID
 * @param id blend mode ID
 * @return blend mode or default if ID does not exist
 */
const BlendMode &findBlendMode(int id);

/**
 * @brief Find the blending mode based on its SVG name
 * @param svgname
 * @return blend mode internal index or -1 if not found
 */
int findBlendModeByName(const QString &svgname);

/**
 * Composite a color using a mask onto an image.
 * @param mode composition mode
 * @param base pixels onto which the color is composited
 * @param color ARGB color value
 * @param mask alpha mask
 * @param w width of composition rectangle
 * @param h height of composition rectangle
 * @param maskskip number of bytes to skip to get to the next line in the mask
 * @param baseskip number of (bytes) to skip to get to the next line in the base
 */
void compositeMask(int mode, quint32 *base, quint32 color, const uchar *mask, int w, int h, int maskskip, int baseskip);

/**
 * Composite two equally big image tiles.
 * @param mode composition mode
 * @param base pixels onto which over is composited
 * @parma over the pixels on top of base
 * @param len number of pixels to blend
 * @param opacity blend opacity (0..255)
 */
void compositePixels(int mode, quint32 *base, const quint32 *over, int len, uchar opacity);

/**
 * Get a weighted average of the pixel data using the mask as the weights
 *
 * @param pixels ARGB pixel data
 * @param mask weight mask
 * @param w width of the sampling rectangle
 * @param h height of the sampling rectangle
 * @param maskskip nubmer of bytes to skip to get to the next line in the mask
 * @param pixelskip number of bytes to skip to get to the enxt line in the pixel data
 * @return [weight sum, red, green, blue, alpha]
 */
std::array<quint32, 5> sampleMask(const quint32 *pixels, const uchar *mask, int w, int h, int maskskip, int pixelskip);

/**
 * Add tint to pixel values
 */
void tintPixels(quint32 *pixels, int len, quint32 tint);

}

#endif

