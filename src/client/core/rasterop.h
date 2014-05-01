/*
   DrawPile - a collaborative drawing program.

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

namespace paintcore {

static const int BLEND_MODES=10;
// Names of each blending mode
extern const char *BLEND_MODE[BLEND_MODES];

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
 * @brief Get the blending mode for the given SVG composite operation name
 * @return blending mode or -1 if operation is not supported
 */
int blendModeSvg(const QString &name);

/**
 * @brief Get the SVG composition operation name for the given blend mode
 * @param blendmode
 * @return name. The normal blend mode is returned if blendmode is invalid
 */
const QString &svgBlendMode(int blendmode);

}

#endif

