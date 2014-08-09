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

#include "floodfill.h"
#include "layerstack.h"
#include "layer.h"

#include <QStack>

namespace paintcore {

namespace {

class Floodfill {
public:
	Floodfill(const LayerStack *image, int sourceLayer, const QColor &color, int colorTolerance) :
		source(image),
		scratch(0, 0, QString(), Qt::transparent, image->size()),
		fill(0, 0, QString(), Qt::transparent, image->size()),
		layer(sourceLayer),
		fillColor(color.rgba()),
		tolerance(colorTolerance)
	{ }

	Tile &scratchTile(int x, int y)
	{
		Tile &t = scratch.rtile(x, y);
		if(t.isNull()) {
			if(layer>0) {
				const Layer *sl = source->getLayer(layer);
				Q_ASSERT(sl);
				const Tile &st = sl->tile(x, y);
				if(st.isNull())
					t = Tile(Qt::transparent);
				else
					t = st;

			} else {
				t = source->getFlatTile(x, y);
			}
		}

		return t;
	}

	Tile &fillTile(int x, int y) {
		Tile &t = fill.rtile(x, y);
		if(t.isNull())
			t = Tile(Qt::transparent);

		return t;
	}

	QRgb colorAt(int x, int y)
	{
		int tx = x / Tile::SIZE;
		int ty = y / Tile::SIZE;
		x = x - tx * Tile::SIZE;
		y = y - ty * Tile::SIZE;

		const Tile &t = scratchTile(tx, ty);

		return t.data()[y*Tile::SIZE + x];
	}

	void setPixel(int x, int y) {
		int tx = x / Tile::SIZE;
		int ty = y / Tile::SIZE;
		x = x - tx * Tile::SIZE;
		y = y - ty * Tile::SIZE;

		int i = y*Tile::SIZE+x;
		scratchTile(tx, ty).data()[i] = fillColor;
		fillTile(tx, ty).data()[i] = fillColor;
	}

	bool isSameColor(QRgb c1, QRgb c2) {
		// TODO better color distance function
		int r = (c1 & 0xff) - (signed int)(c2 & 0xff);
		int g = (c1>>8 & 0xff) - (signed int)(c2>>8 & 0xff);
		int b = (c1>>16 & 0xff) - (signed int)(c2>>16 & 0xff);
		int a = (c1>>24 & 0xff) - (signed int)(c2>>24 & 0xff);
		return r*r + g*g + b*b + a*a <= tolerance * tolerance;
	}

	inline bool isOldColorAt(int x, int y) {
		return isSameColor(colorAt(x, y), oldColor);
	}

	void start(const QPoint &startPoint)
	{
		oldColor = colorAt(startPoint.x(), startPoint.y());
		if(oldColor == fillColor)
			return;

		QStack<QPoint> stack;
		stack.push(startPoint);

		const int w1 = scratch.width()-1;

		while(!stack.isEmpty()) {
			QPoint p = stack.pop();

			const int x = p.x();
			int y = p.y();

			bool spanLeft = false;
			bool spanRight = false;

			while(y>=0 && isOldColorAt(x, y)) --y;
			++y;

			while(y < scratch.height() && isOldColorAt(x, y)) {
				setPixel(x, y);

				if(!spanLeft && x>0 && isOldColorAt(x-1, y)) {
					stack.push(QPoint(x-1, y));
					spanLeft = true;

				} else if(spanLeft && x>0 && !isOldColorAt(x-1, y)) {
					spanLeft = false;

				} else if(!spanRight && x<w1 && isOldColorAt(x+1, y)) {
					stack.push(QPoint(x+1, y));
					spanRight = true;

				} else if(spanRight && x<w1 && !isOldColorAt(x+1, y)) {
					spanRight = false;
				}
				++y;
			}
		}
	}

	FillResult result() const
	{
		FillResult res;
		res.image = fill.toCroppedImage(&res.x, &res.y);
		return res;
	}

private:

	const LayerStack *source;

	// The (optionally) merged tile from the image. This layer is also
	// updated during the fill.
	Layer scratch;

	// The fill layer, containing just the filled pixels
	Layer fill;

	// Layer to operate on. If 0, the merged image is used
	int layer;

	// Fill color
	QRgb fillColor;

	// Seed color
	QRgb oldColor;

	// Color matching tolerance
	int tolerance;
};

}

FillResult floodfill(const LayerStack *image, const QPoint &point, const QColor &color, int tolerance, int layer)
{
	Q_ASSERT(image);
	Q_ASSERT(tolerance>=0);

	Floodfill fill(image, layer, color, tolerance);

	if(point.x() >=0 && point.x() < image->width() && point.y()>=0 && point.y() < image->height())
		fill.start(point);

	return fill.result();
}

}
