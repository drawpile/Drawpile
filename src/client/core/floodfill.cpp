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
#include <QPainter>
#include <QVarLengthArray>

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
		if(isSameColor(oldColor, fillColor))
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

/**
 * @brief find the bounding rectangle of non-transparent content
 *
 * An empty rectangle is returned if the whole image is transparent
 * @param image
 */
QRect findOpaqueBoundingRect(const QImage &image)
{
	Q_ASSERT(image.depth()==32);
	Q_ASSERT(!image.isNull());

	int top=image.height(), bottom=0;
	int left=image.width(), right=0;

	const uchar *alpha = image.bits()+3;
	for(int y=0;y<image.height();++y) {
		for(int x=0;x<image.width();++x, alpha+=4) {
			if(*alpha) {
				if(x<left)
					left=x;
				if(x>right)
					right=x;
				if(y<top)
					top=y;
				if(y>bottom)
					bottom=y;
			}
		}
	}

	if(top>bottom || left>right)
		return QRect();
	return QRect(QPoint(left, top), QPoint(right, bottom));
}

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

FillResult expandFill(const FillResult &input, int expansion, const QColor &color)
{
	if(input.image.isNull() || expansion<1)
		return input;

	Q_ASSERT(input.image.format() == QImage::Format_ARGB32);

	FillResult out;

	const int R = expansion;
	const int D = R*2 + 1;

	// Step 1. Pad image to make sure there is room for expansion
	QRect BOUNDS = findOpaqueBoundingRect(input.image);
	if(BOUNDS.isEmpty())
		return input;

	QImage inputImg;
	if(QRect(0, 0, input.image.width(), input.image.height()).contains(BOUNDS.adjusted(-D,-D,D,D))) {
		// Original image has enough padding around the content
		inputImg = input.image;
		out.x = input.x;
		out.y = input.y;

	} else {
		// Not enough padding: add some
		inputImg = QImage(input.image.width() + 2*D, input.image.height() + 2*D, input.image.format());
		inputImg.fill(0);

		QPainter p(&inputImg);
		p.drawImage(D, D, input.image);

		out.x = input.x - D;
		out.y = input.y - D;
		BOUNDS.translate(D, D);
	}

	// Step 2. Generate convolution matrix for expanding the alpha channel
	QVarLengthArray<int> kernel(D*D);
	int i=0;
	const int RR = R*R;
	for(int y=0;y<D;++y) {
		const int y0 = y-R;
		for(int x=0;x<D;++x) {
			const int x0 = x-R;
			// TODO use a gaussian kernel?
			kernel[i++] = x0*x0 + y0*y0 <= RR;
		}
	}
	Q_ASSERT(i==kernel.length());

	// Step 3. Generate expanded image
	out.image = QImage(inputImg.width(), inputImg.height(), inputImg.format());
	out.image.fill(0);

	const uchar *alphaIn = inputImg.bits()+3;
	const QRgb fillColor = color.rgba();
	quint32 *colorOut = reinterpret_cast<quint32*>(out.image.bits());

	// Optimization: skip extra padding
	QRect expBounds = BOUNDS.adjusted(-R, -R, R+1, R+1);
	Q_ASSERT(QRect(0, 0, inputImg.width(), inputImg.height()).contains(expBounds));

	for(int y=expBounds.top();y<expBounds.bottom();++y) {
		for(int x=expBounds.left();x<expBounds.right();++x) {
			int i=0, a=0;
			for(int ky=y-R;ky<=y+R;++ky) {
				for(int kx=x-R;kx<=x+R;++kx) {
					a += alphaIn[ky*inputImg.bytesPerLine() + 4*kx] * kernel[i++];
				}
			}
			Q_ASSERT(i==kernel.length());

			// TODO adjustable threshold
			if(a>0) {
				int j = y*inputImg.width() + x;
				Q_ASSERT(j <= out.image.width() * out.image.height());
				colorOut[j] = fillColor;
			}
		}
	}

	// Step 4. Crop image in case of negative offset
	// (since the protocol doesn't support signed PutImage coordinates)
	if(out.x<0 || out.y<0) {
		const int cropx = out.x<0 ? -out.x : 0;
		const int cropy = out.y<0 ? -out.y : 0;

		Q_ASSERT(cropx < out.image.width());
		Q_ASSERT(cropy < out.image.height());

		QImage cropped = QImage(out.image.width() - cropx, out.image.height() - cropy, out.image.format());
		for(int y=0;y<cropped.height();++y) {
			memcpy(cropped.scanLine(y), out.image.scanLine(y+cropy) + cropx*4, cropped.width() * 4);
		}

		out.image = cropped;
		out.x += cropx;
		out.y += cropy;
	}

	// All done!
	return out;
}

}
