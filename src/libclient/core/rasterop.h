/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2015 Calle Laakkonen

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
#ifndef PAINTCORE_RASTEROP_H
#define PAINTCORE_RASTEROP_H

#include <Qt>
#include <array>

#include "blendmodes.h"

namespace paintcore {

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
void compositeMask(BlendMode::Mode mode, quint32 *base, quint32 color, const uchar *mask, int w, int h, int maskskip, int baseskip);

/**
 * Composite two equally big image tiles.
 * @param mode composition mode
 * @param base pixels onto which over is composited
 * @parma over the pixels on top of base
 * @param len number of pixels to blend
 * @param opacity blend opacity (0..255)
 */
void compositePixels(BlendMode::Mode mode, quint32 *base, const quint32 *over, int len, uchar opacity);

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

