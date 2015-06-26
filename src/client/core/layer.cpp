/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2014 Calle Laakkonen

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
#include <QPainter>
#include <QImage>
#include <QtConcurrent>
#include <QDataStream>
#include <cmath>

#include "layerstack.h"
#include "layer.h"
#include "tile.h"
#include "brush.h"
#include "brushmask.h"
#include "point.h"
#include "blendmodes.h"
#include "rasterop.h"

namespace paintcore {

namespace {

//! Sample colors at layer edges and return the most frequent color
QColor _sampleEdgeColors(const Layer *layer, bool top, bool right, bool bottom, bool left)
{
	const int STEP = 22;
	QHash<QRgb, int> colorfreq;
	// Sample top & bottom edges
	for(int x=0;x<layer->width();x+=STEP) {
		int count;
		QRgb c;
		if(top) {
			c = layer->pixelAt(x, 0);
			count = colorfreq[c];
			++count;
			colorfreq[c] = count;
		}

		if(bottom) {
			c = layer->pixelAt(x, layer->height()-1);
			count = colorfreq[c];
			++count;
			colorfreq[c] = count;
		}
	}

	// Sample left & right edges
	for(int y=0;y<layer->height();y+=STEP) {
		int count;
		QRgb c;
		if(left) {
			c = layer->pixelAt(0, y);
			count = colorfreq[c];
			++count;
			colorfreq[c] = count;
		}

		if(right) {
			c = layer->pixelAt(layer->width()-1, y);
			count = colorfreq[c];
			++count;
			colorfreq[c] = count;
		}
	}

	// Return the most frequent color
	QRgb color=0;
	int freq=0;
	QHashIterator<QRgb, int> i(colorfreq);
	while(i.hasNext()) {
		i.next();
		if(i.value() > freq) {
			freq = i.value();
			color = i.key();
		}
	}

	return QColor::fromRgba(color);
}

}

/**
 * Construct a layer initialized to a solid color
 * @param owner the stack to which this layer belongs to
 * @param id layer ID
 * @param color layer color
 * @parma size layer size
 */
Layer::Layer(LayerStack *owner, int id, const QString& title, const QColor& color, const QSize& size)
	: _owner(owner), _id(id), _title(title), _width(0), _height(0), _xtiles(0), _ytiles(0),
	_opacity(255), _blend(BlendMode::MODE_NORMAL), _hidden(false)
{
	resize(0, size.width(), size.height(), 0);
	
	if(color.alpha() > 0) {
		_tiles.fill(Tile(color));
	}
}

Layer::Layer(LayerStack *owner, int id, const QSize &size)
	: Layer(owner, id, "", Qt::transparent, size)
{
	// sublayers are used for indirect drawing and previews
}

Layer::Layer(const Layer &layer)
	: _owner(layer._owner), _id(layer.id()), _title(layer._title),
	  _width(layer._width), _height(layer._height),
	  _xtiles(layer._xtiles), _ytiles(layer._ytiles),
	  _tiles(layer._tiles),
	  _opacity(layer._opacity), _blend(layer._blend), _hidden(layer._hidden)
{
	// Hidden and ephemeral layers are not copied, since hiding a sublayer is
	// effectively the same as deleting it and ephemeral layers are not considered
	// part of the true layer content.
	foreach(const Layer *sl, layer._sublayers) {
		if(sl->id() >= 0 && !sl->hidden())
			_sublayers.append(new Layer(*sl));
	}
}

Layer::~Layer() {
	foreach(Layer *sl, _sublayers)
		delete sl;
}

void Layer::resize(int top, int right, int bottom, int left)
{
	// Minimize amount of data that needs to be copied
	optimize();

	// Resize sublayers
	foreach(Layer *sl, _sublayers)
		sl->resize(top, right, bottom, left);

	// Calculate new size
	int width = left + _width + right;
	int height = top + _height + bottom;

	int xtiles = Tile::roundTiles(width);
	int ytiles = Tile::roundTiles(height);
	QVector<Tile> tiles(xtiles * ytiles);

	// if there is no old content, resizing is simple
	bool hascontent = false;
	for(int i=0;i<_tiles.count();++i) {
		if(!_tiles.at(i).isBlank()) {
			hascontent = true;
			break;
		}
	}
	if(!hascontent) {
		_width = width;
		_height = height;
		_xtiles = xtiles;
		_ytiles = ytiles;
		_tiles = tiles;
		return;
	}

	// Sample colors around the layer edges to determine fill color
	// for the new tiles
	Tile bgtile;
	{
		QColor bgcolor = _sampleEdgeColors(this, top>0, right>0, bottom>0, left>0);
		if(bgcolor.alpha()>0)
			bgtile = Tile(bgcolor);
	}

	if((left % Tile::SIZE) || (top % Tile::SIZE)) {
		// If top/left adjustment is not divisble by tile size,
		// we need to move the layer content

		QImage oldcontent = toImage();

		_width = width;
		_height = height;
		_xtiles = xtiles;
		_ytiles = ytiles;
		_tiles = tiles;
		if(left<0 || top<0) {
			int cropx = 0;
			if(left<0) {
				cropx = -left;
				left = 0;
			}
			int cropy = 0;
			if(top<0) {
				cropy = -top;
				top = 0;
			}
			oldcontent = oldcontent.copy(cropx, cropy, oldcontent.width()-cropx, oldcontent.height()-cropy);
		}

		_tiles.fill(bgtile);

		putImage(left, top, oldcontent, BlendMode::MODE_REPLACE);

	} else {
		// top/left offset is aligned at tile boundary:
		// existing tile content can be reused

		const int firstrow = Tile::roundTiles(-top);
		const int firstcol = Tile::roundTiles(-left);

		int oldy = firstrow;
		for(int y=0;y<ytiles;++y,++oldy) {
			int oldx = firstcol;
			const int oldyy = _xtiles * oldy;
			const int yy = xtiles * y;
			for(int x=0;x<xtiles;++x,++oldx) {
				const int i = yy + x;

				if(oldy<0 || oldy>=_ytiles || oldx<0 || oldx>=_xtiles) {
					tiles[i] = bgtile;

				} else {
					const int oldi = oldyy + oldx;
					tiles[i] = _tiles.at(oldi);
				}
			}
		}

		_width = width;
		_height = height;
		_xtiles = xtiles;
		_ytiles = ytiles;
		_tiles = tiles;
	}
}

void Layer::setTitle(const QString& title)
{
	_title = title;
}

QImage Layer::toImage() const {
	QImage image(_width, _height, QImage::Format_ARGB32);
	int i=0;
	for(int y=0;y<_ytiles;++y) {
		for(int x=0;x<_xtiles;++x,++i)
			_tiles[i].copyToImage(image, x*Tile::SIZE, y*Tile::SIZE);
	}
	return image;
}

QImage Layer::toCroppedImage(int *xOffset, int *yOffset) const
{
	int top=_ytiles, bottom=0;
	int left=_xtiles, right=0;

	// Find bounding rectangle of non-blank tiles
	for(int y=0;y<_ytiles;++y) {
		for(int x=0;x<_xtiles;++x) {
			const Tile &t = _tiles.at(y*_xtiles + x);
			if(!t.isBlank()) {
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

	if(top==_ytiles) {
		// Entire layer appears to be blank
		return QImage();
	}

	// Copy tiles to image
	QImage image((right-left+1)*Tile::SIZE, (bottom-top+1)*Tile::SIZE, QImage::Format_ARGB32);
	for(int y=top;y<=bottom;++y) {
		for(int x=left;x<=right;++x) {
			_tiles.at(y*_xtiles+x).copyToImage(image, (x-left)*Tile::SIZE, (y-top)*Tile::SIZE);
		}
	}

	if(xOffset)
		*xOffset = left * Tile::SIZE;
	if(yOffset)
		*yOffset = top * Tile::SIZE;

	// TODO pixel perfect cropping
	return image;
}

/**
 * @param x
 * @param y
 * @return invalid color if x or y is outside image boundaries
 */
QColor Layer::colorAt(int x, int y) const
{
	if(x<0 || y<0 || x>=_width || y>=_height)
		return QColor();

	return QColor::fromRgb(pixelAt(x, y));
}

QRgb Layer::pixelAt(int x, int y) const
{
	if(x<0 || y<0 || x>=_width || y>=_height)
		return 0;

	const int yindex = y/Tile::SIZE;
	const int xindex = x/Tile::SIZE;

	return tile(xindex, yindex).pixel(x-xindex*Tile::SIZE, y-yindex*Tile::SIZE);
}

/**
 * @param opacity
 */
void Layer::setOpacity(int opacity)
{
	Q_ASSERT(opacity>=0 && opacity<256);
	_opacity = opacity;
	markOpaqueDirty(true);
}

void Layer::setBlend(BlendMode::Mode blend)
{
	_blend = blend;
	markOpaqueDirty();
}

/**
 * @param hide new status
 */
void Layer::setHidden(bool hide)
{
	_hidden = hide;
	markOpaqueDirty(true);
}

/**
 * Return a temporary layer with the original image padded and composited with the
 * content of this layer.
 *
 * @param xpos target image position
 * @param ypos target image position
 * @param original the image to pad
 * @param mode compositing mode
 */
Layer Layer::padImageToTileBoundary(int xpos, int ypos, const QImage &original, BlendMode::Mode mode) const
{
	const int x0 = Tile::roundDown(xpos);
	const int x1 = qMin(_width, Tile::roundUp(xpos+original.width()));
	const int y0 = Tile::roundDown(ypos);
	const int y1 = qMin(_height, Tile::roundUp(ypos+original.height()));

	const int w = x1 - x0;
	const int h = y1 - y0;
	
	// Pad the image to tile boundaries
	QImage image;
	if(image.width() == w && image.height() == h) {
		image = original;

	} else {
		image = QImage(w, h, QImage::Format_ARGB32);
		QPainter painter(&image);

		if(mode == BlendMode::MODE_REPLACE) {
			// Replace mode is special: we must copy the original pixels
			// and do the composition with QPainter, since layer merge
			// can't distinguish the padding from image transparency
			for(int y=0;y<h;y+=Tile::SIZE) {
				for(int x=0;x<w;x+=Tile::SIZE) {
					tile((x0+x) / Tile::SIZE, (y0+y) / Tile::SIZE).copyToImage(image, x, y);
				}
			}
			painter.setCompositionMode(QPainter::CompositionMode_Source);

		} else {
			image.fill(0);
		}

		painter.drawImage(xpos-x0, ypos-y0, original);
#if 0 /* debugging aid */
		painter.setPen(Qt::red);
		painter.drawLine(0, 0, w-1, 0);
		painter.drawLine(0, 0, 0, h-1);
		painter.drawLine(w-1, 0, w-1, h-1);
		painter.drawLine(0, h-1, w-1, h-1);
#endif
	}

	// Create scratch layers and composite
	Layer scratch(nullptr, 0, QString(), Qt::transparent, QSize(w,h));
	Layer imglayer = scratch;

	// Copy image pixels to image layer
	for(int y=0;y<h;y+=Tile::SIZE) {
		for(int x=0;x<w;x+=Tile::SIZE) {
			imglayer.rtile(x/Tile::SIZE, y/Tile::SIZE) = Tile(image, x, y);
		}
	}

	// In replace mode, compositing was already done with QPainter
	if(mode == BlendMode::MODE_REPLACE) {
		return imglayer;
	}

	// Copy tiles from current layer to scratch layer
	for(int y=0;y<h;y+=Tile::SIZE) {
		for(int x=0;x<w;x+=Tile::SIZE) {
			scratch.rtile(x/Tile::SIZE, y/Tile::SIZE) = tile((x+x0)/Tile::SIZE, (y+y0)/Tile::SIZE);
		}
	}

	// Merge image using standard layer compositing ops
	imglayer.setBlend(mode);

	scratch.merge(&imglayer);
	return scratch;
}

/**
 * @param x x coordinate
 * @param y y coordinate
 * @param image the image to draw
 * @param mode blending/compositing mode (see protocol::PutImage)
 */
void Layer::putImage(int x, int y, QImage image, BlendMode::Mode mode)
{
	Q_ASSERT(image.format() == QImage::Format_ARGB32);

	// Protocol uses unsigned coordinates, so we don't need to support
	// negative values here either.
	Q_ASSERT(x>=0 && y>=0);
	if(x<0 || y<0)
		return;
	
	// Check if the image is completely outside the layer
	if(x >= _width || y >= _height)
		return;

	const int x0 = Tile::roundDown(x);
	const int y0 = Tile::roundDown(y);
	
	Layer imageLayer = padImageToTileBoundary(x, y, image, mode);

	// Replace this layer's tiles with the scratch tiles
	const int tx0 = x0 / Tile::SIZE;
	const int ty0 = y0 / Tile::SIZE;
	const int tx1 = qMin((x0 + imageLayer.width() - 1) / Tile::SIZE, _xtiles-1);
	const int ty1 = qMin((y0 + imageLayer.height() - 1) / Tile::SIZE, _ytiles-1);

	for(int ty=ty0;ty<=ty1;++ty) {
		for(int tx=tx0;tx<=tx1;++tx) {
			rtile(tx, ty) = imageLayer.tile(tx-tx0, ty-ty0);
		}
	}
	
	if(_owner && visible()) {
		_owner->markDirty(QRect(x, y, image.width(), image.height()));
		_owner->notifyAreaChanged();
	}
}

void Layer::fillRect(const QRect &rectangle, const QColor &color, BlendMode::Mode blendmode)
{
	const QRect canvas(0, 0, _width, _height);

	if(rectangle.contains(canvas) && blendmode==BlendMode::MODE_REPLACE) {
		// Special case: overwrite whole layer
		_tiles.fill(Tile(color));

	} else {
		// The usual case: only a portion of the layer is filled or pixel blending is needed
		QRect rect = rectangle.intersected(canvas);

		uchar mask[Tile::LENGTH];
		if(blendmode==255)
			memset(mask, 0xff, Tile::LENGTH);
		else
			memset(mask, color.alpha(), Tile::LENGTH);

		const int size = Tile::SIZE;
		const int bottom = rect.y() + rect.height();
		const int right = rect.x() + rect.width();

		const int tx0 = rect.x() / size;
		const int tx1 = (right-1) / size;
		const int ty0 = rect.y() / size;
		const int ty1 = (bottom-1) / size;

		bool canIncrOpacity;
		if(blendmode==255 && color.alpha()==0)
			canIncrOpacity = false;
		else
			canIncrOpacity = findBlendMode(blendmode).flags.testFlag(BlendMode::IncrOpacity);

		for(int ty=ty0;ty<=ty1;++ty) {
			for(int tx=tx0;tx<=tx1;++tx) {
				int left = qMax(tx * size, rect.x()) - tx*size;
				int top = qMax(ty * size, rect.y()) - ty*size;
				int w = qMin((tx+1)*size, right) - tx*size - left;
				int h = qMin((ty+1)*size, bottom) - ty*size - top;

				Tile &t = _tiles[ty*_xtiles+tx];

				if(!t.isNull() || canIncrOpacity)
					t.composite(blendmode, mask, color, left, top, w, h, 0);
			}
		}
	}

	if(_owner && visible()) {
		_owner->markDirty(rectangle);
		_owner->notifyAreaChanged();
	}
}

void Layer::dab(int contextId, const Brush &brush, const Point &point, StrokeState &state)
{
	Brush effective_brush = brush;
	Layer *l = this;

	if(!brush.incremental()) {
		// Indirect brush: use a sublayer
		l = getSubLayer(contextId, brush.blendingMode(), brush.opacity(1) * 255);

		effective_brush.setOpacity(1.0);
		effective_brush.setOpacity2(brush.isOpacityVariable() ? 0.0 : 1.0);
		effective_brush.setBlendingMode(BlendMode::MODE_NORMAL);

	} else if(contextId<0) {
		// Special case: negative context IDs are temporary overlay strokes
		l = getSubLayer(contextId, brush.blendingMode(), 255);
		effective_brush.setBlendingMode(BlendMode::MODE_NORMAL);
	}

	Point p = point;
	if(!effective_brush.subpixel()) {
		p.setX(qFloor(p.x()));
		p.setY(qFloor(p.y()));
	}

	l->directDab(effective_brush, p, state);

	if(_owner)
		_owner->notifyAreaChanged();
}

/**
 * Draw a line using either drawSoftLine or drawHardLine, depending on
 * the subpixel hint of the brush.
 * @param context drawing context id (needed for indirect drawing)
 */
void Layer::drawLine(int contextId, const Brush& brush, const Point& from, const Point& to, StrokeState &state)
{
	Brush effective_brush = brush;
	Layer *l = this;

	if(!brush.incremental()) {
		// Indirect brush: use a sublayer
		l = getSubLayer(contextId, brush.blendingMode(), brush.opacity(1) * 255);

		effective_brush.setOpacity(1.0);
		effective_brush.setOpacity2(brush.isOpacityVariable() ? 0.0 : 1.0);
		effective_brush.setBlendingMode(BlendMode::MODE_NORMAL);

	} else if(contextId<0) {
		// Special case: negative context IDs are temporary overlay strokes
		l = getSubLayer(contextId, brush.blendingMode(), 255);
		effective_brush.setBlendingMode(BlendMode::MODE_NORMAL);
	}

	if(effective_brush.subpixel())
		l->drawSoftLine(effective_brush, from, to, state);
	else
		l->drawHardLine(effective_brush, from, to, state);

	if(_owner)
		_owner->notifyAreaChanged();
}

/**
 * This function is optimized for drawing with subpixel precision.
 * @param brush brush to draw the line with
 * @param from starting point
 * @param to ending point
 * @param distance distance from previous dab.
 */
void Layer::drawSoftLine(const Brush& brush, const Point& from, const Point& to, StrokeState &state)
{
	qreal dx = to.x() - from.x();
	qreal dy = to.y() - from.y();
	const qreal dist = hypot(dx, dy);
	dx = dx / dist;
	dy = dy / dist;
	const qreal dp = (to.pressure() - from.pressure()) / dist;

	const qreal spacing0 = qMax(1.0, brush.spacingDist(from.pressure()));
	qreal i;
	if(state.distance>=spacing0)
		i = 0;
	else if(state.distance==0)
		i = spacing0;
	else
		i = state.distance;

	Point p(from.x() + dx*i, from.y() + dy*i, qBound(0.0, from.pressure() + dp*i, 1.0));

	while(i<=dist) {
		const qreal spacing = qMax(1.0, brush.spacingDist(p.pressure()));
		directDab(brush, p, state);
		p.rx() += dx * spacing;
		p.ry() += dy * spacing;
		p.setPressure(qBound(0.0, p.pressure() + dp * spacing, 1.0));
		i += spacing;
	}
	state.distance = i-dist;
}

/**
 * This line drawing function is optimized for drawing with no subpixel
 * precision.
 * The last point is not drawn, so successive lines can be drawn blotches.
 */
void Layer::drawHardLine(const Brush &brush, const Point& from, const Point& to, StrokeState &state) {
	const qreal dp = (to.pressure()-from.pressure()) / hypot(to.x()-from.x(), to.y()-from.y());

	int x0 = qFloor(from.x());
	int y0 = qFloor(from.y());
	qreal p = from.pressure();
	int x1 = qFloor(to.x());
	int y1 = qFloor(to.y());
	int dy = y1 - y0;
	int dx = x1 - x0;
	int stepx, stepy;

	if (dy < 0) {
		dy = -dy;
		stepy = -1;
	} else {
		stepy = 1;
	}
	if (dx < 0) {
		dx = -dx;
		stepx = -1;
	} else {
		stepx = 1;
	}

	dy *= 2;
	dx *= 2;

	qreal distance = state.distance;

	if (dx > dy) {
		int fraction = dy - (dx >> 1);
		while (x0 != x1) {
			const qreal spacing = brush.spacingDist(p);
			if (fraction >= 0) {
				y0 += stepy;
				fraction -= dx;
			}
			x0 += stepx;
			fraction += dy;
			if(++distance >= spacing) {
				directDab(brush, Point(x0, y0, p), state);
				distance = 0;
			}
			p += dp;
		}
	} else {
		int fraction = dx - (dy >> 1);
		while (y0 != y1) {
			const qreal spacing = brush.spacingDist(p);
			if (fraction >= 0) {
				x0 += stepx;
				fraction -= dy;
			}
			y0 += stepy;
			fraction += dx;
			if(++distance >= spacing) {
				directDab(brush, Point(x0, y0, p), state);
				distance = 0;
			}
			p += dp;
		}
	}

	state.distance = distance;
}

/**
 * Apply a single dab of the brush to the layer
 * @param brush brush to use
 * @param point where to dab. May be outside the image.
 * @param sampleSmudgeColor if true (and smudging is enabled for the brush), sample the layer color before applying dab
 * @param state stroke state (used for the smudge color)
 */
void Layer::directDab(const Brush &brush, const Point& point, StrokeState &state)
{
	// Render the brush
	const BrushStamp bs = makeGimpStyleBrushStamp(brush, point);
	const int top=bs.top, left=bs.left;
	const int dia = bs.mask.diameter();
	const int bottom = qMin(top + dia, _height);
	const int right = qMin(left + dia, _width);

	if(left+dia<=0 || top+dia<=0 || left>=_width || top>=_height)
		return;

	const qreal smudge = brush.smudge(point.pressure());

	if(++state.smudgeDistance > brush.resmudge() && smudge>0) {
		const QColor sampled = getDabColor(bs);

		const qreal a = sampled.alphaF() * smudge;

		state.smudgeColor = QColor::fromRgbF(
			state.smudgeColor.redF() * (1-a) + sampled.redF() * a,
			state.smudgeColor.greenF() * (1-a) + sampled.greenF() * a,
			state.smudgeColor.blueF() * (1-a) + sampled.blueF() * a
		);
		state.smudgeDistance = 0;
	}

	// Composite the brush mask onto the layer
	const uchar *values = bs.mask.data();
	QColor color = smudge > 0 ? state.smudgeColor : brush.color(point.pressure());

	// A single dab can (and often does) span multiple tiles.
	int y = top<0?0:top;
	int yb = top<0?-top:0; // y in relation to brush origin
	const int x0 = left<0?0:left;
	const int xb0 = left<0?-left:0;
	while(y<bottom) {
		const int yindex = y / Tile::SIZE;
		const int yt = y - yindex * Tile::SIZE;
		const int hb = yt+dia-yb < Tile::SIZE ? dia-yb : Tile::SIZE-yt;
		int x = x0;
		int xb = xb0; // x in relation to brush origin
		while(x<right) {
			const int xindex = x / Tile::SIZE;
			const int xt = x - xindex * Tile::SIZE;
			const int wb = xt+dia-xb < Tile::SIZE ? dia-xb : Tile::SIZE-xt;
			const int i = _xtiles * yindex + xindex;
			_tiles[i].composite(
					brush.blendingMode(),
					values + yb * dia + xb,
					color,
					xt, yt,
					wb, hb,
					dia-wb
					);

			x = (xindex+1) * Tile::SIZE;
			xb = xb + wb;
		}
		y = (yindex+1) * Tile::SIZE;
		yb = yb + hb;
	}

	if(_owner && visible())
		_owner->markDirty(QRect(left, top, right-left, bottom-top));

}

/**
 * @brief Get a weighted average of the layer's color, using the given brush mask as the weight
 * @param stamp
 * @return color average
 */
QColor Layer::getDabColor(const BrushStamp &stamp) const
{
	// This is very much like directDab, instead we only read pixel values
	const uchar *weights = stamp.mask.data();

	// The mask can overlap multiple tiles
	const int dia = stamp.mask.diameter();
	const int bottom = qMin(stamp.top + dia, _height);
	const int right = qMin(stamp.left + dia, _width);

	int y = qMax(0, stamp.top);
	int yb = stamp.top<0?-stamp.top:0; // y in relation to brush origin
	const int x0 = qMax(0, stamp.left);
	const int xb0 = stamp.left<0?-stamp.left:0;

	qreal weight=0, red=0, green=0, blue=0, alpha=0;

	// collect weighted color sums
	while(y<bottom) {
		const int yindex = y / Tile::SIZE;
		const int yt = y - yindex * Tile::SIZE;
		const int hb = yt+dia-yb < Tile::SIZE ? dia-yb : Tile::SIZE-yt;
		int x = x0;
		int xb = xb0; // x in relation to brush origin
		while(x<right) {
			const int xindex = x / Tile::SIZE;
			const int xt = x - xindex * Tile::SIZE;
			const int wb = xt+dia-xb < Tile::SIZE ? dia-xb : Tile::SIZE-xt;
			const int i = _xtiles * yindex + xindex;

			std::array<quint32, 5> avg = _tiles.at(i).weightedAverage(weights + yb * dia + xb, xt, yt, wb, hb, dia-wb);
			weight += avg[0];
			red += avg[1];
			green += avg[2];
			blue += avg[3];
			alpha += avg[4];

			x = (xindex+1) * Tile::SIZE;
			xb = xb + wb;
		}
		y = (yindex+1) * Tile::SIZE;
		yb = yb + hb;
	}

	// Calculate final average
	red /= weight;
	green /= weight;
	blue /= weight;
	alpha /= weight;

	// Unpremultiply
	if(alpha>0) {
		red = qMin(1.0, red/alpha);
		green = qMin(1.0, green/alpha);
		blue = qMin(1.0, blue/alpha);
	}

	Q_ASSERT(red >= 0 && red <= 1);
	Q_ASSERT(green >= 0 && green <= 1);
	Q_ASSERT(blue >= 0 && blue <= 1);
	Q_ASSERT(alpha >= 0 && alpha <= 1);
	return QColor::fromRgbF(red, green, blue, alpha);
}

/**
 * @param layer the layer that will be merged to this
 * @param sublayers merge sublayers as well
 */
void Layer::merge(const Layer *layer, bool sublayers)
{
	Q_ASSERT(layer->_xtiles == _xtiles);
	Q_ASSERT(layer->_ytiles == _ytiles);

	// Gather a list of non-null tiles to merge
	QVector<int> mergeidx;
	mergeidx.reserve(_tiles.size());
	for(int i=0;i<_tiles.size();++i) {
		bool isnull = layer->_tiles[i].isNull();

		if(isnull && sublayers) {
			foreach(Layer *sl, _sublayers) {
				if(sl->_tiles[i].isNull()) {
					isnull = false;
					break;
				}
			}
		}

		if(!isnull)
			mergeidx.append(i);
	}

	// Detach tile vector explicitly to make sure concurrent modifications
	// are all done to the same vector
	_tiles.detach();

	// Merge tiles
	QtConcurrent::blockingMap(mergeidx, [this, layer, sublayers](int idx) {
		if(sublayers) {
			Tile t = layer->_tiles[idx];

			for(Layer *sl : layer->_sublayers) {
				if(sl->visible()) {
					t.merge(sl->_tiles[idx], sl->_opacity, sl->blendmode());
				}
			}
			_tiles[idx].merge(t, layer->_opacity, layer->blendmode());

		} else {
			_tiles[idx].merge(layer->_tiles[idx], layer->_opacity, layer->blendmode());
		}
	});

	// Merging a layer does not cause an immediate visual change, so we don't
	// mark the area as dirty here.
}

/**
 * Free all tiles that are completely transparent
 */
void Layer::optimize()
{
	// Optimize tile memory usage
	for(int i=0;i<_tiles.size();++i) {
		if(!_tiles[i].isNull() && _tiles[i].isBlank())
			_tiles[i] = Tile();
	}

	// Delete unused sublayers
	QMutableListIterator<Layer*> li(_sublayers);
	while(li.hasNext()) {
		Layer *sl = li.next();
		if(sl->hidden()) {
			delete sl;
			li.remove();
		}
	}
}

void Layer::makeBlank()
{
	_tiles.fill(Tile());

	if(_owner && visible())
		_owner->markDirty();
}

/**
 * @brief Get or create a new sublayer
 *
 * Sublayers are temporary layers used for indirect drawing.
 *
 * Negative IDs are used for ephemeral preview layers.
 *
 * @param id sublayer ID (unique to parent layer only)
 * @param opacity layer opacity (set when creating the layer)
 * @return sublayer
 */
Layer *Layer::getSubLayer(int id, BlendMode::Mode blendmode, uchar opacity)
{
	// See if the sublayer exists already
	foreach(Layer *sl, _sublayers)
		if(sl->id() == id) {
			if(sl->hidden()) {
				// Hidden, reset properties
				sl->makeBlank();
				sl->_opacity = opacity;
				sl->_blend = blendmode;
				sl->_hidden = false;
			}
			return sl;
		}

	// Okay, try recycling a sublayer
	foreach(Layer *sl, _sublayers) {
		if(sl->hidden()) {
			// Set these flags directly to avoid markDirty call.
			// We know the layer is invisible at this point
			sl->makeBlank();
			sl->_id = id;
			sl->_opacity = opacity;
			sl->_blend = blendmode;
			sl->_hidden = false;
			return sl;
		}
	}

	// No available sublayers, create a new one
	Layer *sl = new Layer(_owner, id, QSize(_width, _height));
	sl->_opacity = opacity;
	sl->_blend = blendmode;
	_sublayers.append(sl);
	return sl;
}

/**
 * This is used to end an indirect stroke.
 * If a sublayer with the given ID does not exist, this function does nothing.
 * @param id
 */
void Layer::mergeSublayer(int id)
{
	foreach(Layer *sl, _sublayers) {
		if(sl->id() == id) {
			if(!sl->hidden()) {
				merge(sl);
				// Set hidden flag directly to avoid markDirty call.
				// The merge should cause no visual change.
				sl->_hidden = true;
			}
			return;
		}
	}
}

/**
 * @brief This is used to remove temporary sublayers
 *
 * The layer isn't actually deleted, but is just marked as hidden for recycling.
 * Call optimize() to clean up removed sublayers.
 * @param id
 */
void Layer::removeSublayer(int id)
{
	foreach(Layer *sl, _sublayers) {
		if(sl->id() == id) {
			sl->setHidden(true);
			return;
		}
	}
}

void Layer::markOpaqueDirty(bool forceVisible)
{
	if(!_owner || !(forceVisible || visible()))
		return;

	for(int i=0;i<_xtiles*_ytiles;++i) {
		if(!_tiles.at(i).isNull())
			_owner->markDirty(i);
	}
	_owner->notifyAreaChanged();
}

void Layer::toDatastream(QDataStream &out) const
{
	// Write ID
	out << qint32(id());

	// Write title
	out << _title;

	// Write opacity, blend mode and hidden flag
	out << _opacity;
	out << quint8(_blend);
	out << _hidden;

	// Write layer data
	out << toImage();

	// Write sublayers
	out << quint8(_sublayers.size());
	foreach(const Layer *sl, _sublayers) {
		sl->toDatastream(out);
	}
}

Layer *Layer::fromDatastream(LayerStack *owner, QDataStream &in)
{
	// Read ID
	qint32 id;
	in >> id;

	// Read title
	QString title;
	in >> title;

	// Read opacity, blend mode and hidden flag
	uchar opacity;
	uchar blend;
	bool hidden;
	in >> opacity >> blend >> hidden;

	// Read image data
	QImage img;
	in >> img;

	Layer *layer = new Layer(owner, id, title, Qt::transparent, img.size());
	layer->_opacity = opacity;
	layer->_blend = BlendMode::Mode(blend);
	layer->_hidden = hidden;
	layer->putImage(0, 0, img, BlendMode::MODE_REPLACE);

	// Read sublayers
	quint8 sublayers;
	in >> sublayers;
	while(sublayers--) {
		Layer *sl = Layer::fromDatastream(owner, in);
		if(!sl) {
			delete layer;
			return 0;
		}
		layer->_sublayers.append(sl);
	}

	return layer;
}

}
