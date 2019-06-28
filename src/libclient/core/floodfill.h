/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2017 Calle Laakkonen

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

#ifndef FLOODFILL_H
#define FLOODFILL_H

#include <QImage>

namespace paintcore {

class LayerStack;

struct FillResult {
	//! The fill bitmap.
	QImage image;

	//! bitmap X offset
	int x;

	//! bitmap Y offset
	int y;

	//! The pixel value of the point on the target layer where the fill started
	QRgb layerSeedColor;

	//! Was the fill aborted due to size limit being reaced?
	bool oversize;

	FillResult() : x(0), y(0), layerSeedColor(0), oversize(false) { }
};

/**
 * @brief Perform a flood fill on the image
 *
 * If the fill color is transparent, either black or white will be used, depending on the color at the starting point.
 *
 * @param image the image on which to perform the fill
 * @param point fill seed point
 * @param color fill color
 * @param tolerance color matching tolerance
 * @param layer the active layer
 * @param merge if true, use merged pixel values from all layers
 * @param sizelimit maximum number of pixels to color (aborts fill if exceeded)
 * @return fill bitmap
 */
FillResult floodfill(const LayerStack *image, const QPoint &point, const QColor &color, int tolerance, int layer, bool merge, unsigned int sizelimit);

/**
 * @brief Take a previous flood fill result and expand the filled area
 *
 * This is useful when coloring lineart with soft edges.
 *
 * @param input fill area to expand
 * @param expansion expansion kernel radius
 * @param color expansion color
 * @return expanded fill result
 */
FillResult expandFill(const FillResult &input, int expansion, const QColor &color);

}

#endif // FLOODFILL_H
