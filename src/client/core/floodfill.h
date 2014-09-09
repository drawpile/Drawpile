/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
};

/**
 * @brief Perform a flood fill on the image
 *
 * @param image the image on which to perform the fill
 * @param point fill seed point
 * @param color fill color
 * @param tolerance color matching tolerance
 * @param layer the layer to operate on. If 0, the whole merged image is used
 * @return fill bitmap
 */
FillResult floodfill(const LayerStack *image, const QPoint &point, const QColor &color, int tolerance, int layer);

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
