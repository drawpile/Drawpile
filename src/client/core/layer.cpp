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
#include <QDebug>
#include <QPainter>
#include <QImage>
#include <cmath>

#include "layerstack.h"
#include "layer.h"
#include "tile.h"
#include "brush.h"
#include "point.h"

namespace dpcore {

/**
 * Construct a layer initialized to a solid color
 * @param owner the stack to which this layer belongs to
 * @param id layer ID
 * @param color layer color
 * @parma size layer size
 */
Layer::Layer(LayerStack *owner, int id, const QString& title, const QColor& color, const QSize& size)
	: owner_(owner), id_(id), _title(title), width_(size.width()), height_(size.height()),
	_opacity(255), _blend(1), _hidden(false)
{
	_xtiles = (width_+Tile::SIZE-1) / Tile::SIZE;
	_ytiles = (height_+Tile::SIZE-1) / Tile::SIZE;
	_tiles = new Tile*[_xtiles * _ytiles];
	
	if(color.alpha() == 0) {
		// Blank layer
		for(int i=0;i<_xtiles*_ytiles;++i)
			_tiles[i] = 0;
	} else {
		// Solid fill
		for(int y=0;y<_ytiles;++y)
			for(int x=0;x<_xtiles;++x)
				_tiles[y*_xtiles+x] = new Tile(color, x, y);
	}
}

Layer::Layer(LayerStack *owner, int id, const QSize &size)
	: Layer(owner, id, "", Qt::transparent, size)
{
	// sublayers are used for indirect drawing
}

Layer::~Layer() {
	for(int i=0;i<_xtiles*_ytiles;++i)
		delete _tiles[i];
	delete [] _tiles;
}

void Layer::setTitle(const QString& title)
{
	_title = title;
}

QImage Layer::toImage() const {
	QImage image(width_, height_, QImage::Format_ARGB32);
	image.fill(0);
	for(int i=0;i<_xtiles*_ytiles;++i) {
		if(_tiles[i])
			_tiles[i]->copyToImage(image);
	}
	return image;
}

/**
 * @param x
 * @param y
 * @return invalid color if x or y is outside image boundaries
 */
QColor Layer::colorAt(int x, int y) const
{
	if(x<0 || y<0 || x>=width_ || y>=height_)
		return QColor();
	const int yindex = y/Tile::SIZE;
	const int xindex = x/Tile::SIZE;
	const Tile *t = tile(xindex, yindex);
	if(!t)
		return Qt::transparent;
	
	return QColor::fromRgb(t->pixel(x-xindex*Tile::SIZE, y-yindex*Tile::SIZE));
}

/**
 * @param opacity
 */
void Layer::setOpacity(int opacity)
{
	Q_ASSERT(opacity>=0 && opacity<256);
	_opacity = opacity;
	// TODO optimization: mark only nonempty tiles
	if(owner_ && visible())
		owner_->markDirty();
}

void Layer::setBlend(int blend)
{
	_blend = blend;
	// TODO optimization: mark only nonempty tiles
	if(owner_ && visible())
		owner_->markDirty();
}

/**
 * @param hide new status
 */
void Layer::setHidden(bool hide)
{
	_hidden = hide;
	// TODO same optimization as above
	if(owner_ && _opacity>0)
		owner_->markDirty();
}

/**
 * Return a copy of the image with the borders padded to align with tile boundaries.
 * The padding pixels are taken from the layer content, so the image can be used
 * to replace the existing tiles.
 * @param xpos target image position
 * @param ypos target image position
 * @param original the image to pad
 * @param alpha alpha blend image
 */
QImage Layer::padImageToTileBoundary(int xpos, int ypos, const QImage &original, bool alpha) const
{
	const int x0 = Tile::roundDown(xpos);
	const int x1 = Tile::roundUp(xpos+original.width());
	const int y0 = Tile::roundDown(ypos);
	const int y1 = Tile::roundUp(ypos+original.height());

	const int w = x1 - x0;
	const int h = y1 - y0;
	
	QImage image(w, h, QImage::Format_ARGB32);
	image.fill(0);

	// Copy background from existing tiles
	for(int y=0;y<h;y+=Tile::SIZE) {
		//int yt = (y0 + y) / Tile::SIZE;
		for(int x=0;x<w;x+=Tile::SIZE) {
			const Tile *t = tile((x0+x) / Tile::SIZE, (y0+y) / Tile::SIZE);
			if(t)
				t->copyToImage(image, x, y);
		}
	}
	
	// Paint new image
	QPainter painter(&image);
	if(!alpha)
		painter.setCompositionMode(QPainter::CompositionMode_Source);
	painter.drawImage(xpos-x0, ypos-y0, original);
	
#if 0 /* debugging aid */
	painter.setPen(Qt::red);
	painter.drawLine(0, 0, w-1, 0);
	painter.drawLine(0, 0, 0, h-1);
	painter.drawLine(w-1, 0, w-1, h-1);
	painter.drawLine(0, h-1, w-1, h-1);
#endif
	
	return image;
}

/**
 * @param x x coordinate
 * @param y y coordinate
 * @param image the image to draw
 * @param blend use alpha blending
 */
void Layer::putImage(int x, int y, QImage image, bool blend)
{
	Q_ASSERT(image.format() == QImage::Format_ARGB32);
	
	const int x0 = Tile::roundDown(x);
	const int y0 = Tile::roundDown(y);
	const int xoff = x - x0;
	const int yoff = y - y0;
	
	if(xoff || yoff || image.width() % Tile::SIZE || image.height() % Tile::SIZE || blend) {
		image = padImageToTileBoundary(x, y, image, blend);
	}
	
	const int tx0 = x / Tile::SIZE;
	const int tx1 = tx0 + (image.width()-1) / Tile::SIZE;
	int ty0 = y / Tile::SIZE;
	const int ty1 = ty0 + (image.height()-1) / Tile::SIZE;
	
	for(;ty0<=ty1;++ty0) {
		for(int tx=tx0;tx<=tx1;++tx) {
			int i = ty0*_xtiles + tx;
			Q_ASSERT(i>=0 && i < _xtiles*_ytiles);
			delete _tiles[i];
			_tiles[i] = new Tile(image, tx, ty0, Tile::roundDown(x), Tile::roundDown(y));
		}
	}
	
	if(owner_ && visible())
		owner_->markDirty(QRect(x, y, image.width(), image.height()));
}

void Layer::dab(int contextId, const Brush &brush, const Point &point)
{
	if(!brush.incremental()) {
		// Indirect brush: use a sublayer
		Layer *sl = getSubLayer(contextId, brush.blendingMode(), brush.opacity(1) * 255);

		Brush slb(brush);
		slb.setOpacity(1.0);
		slb.setOpacity2(brush.isOpacityVariable() ? 0.0 : 1.0);
		slb.setBlendingMode(1);

		sl->directDab(slb, point);
	} else {
		directDab(brush, point);
	}
}

/**
 * Draw a line using either drawSoftLine or drawHardLine, depending on
 * the subpixel hint of the brush.
 * @param context drawing context id (needed for indirect drawing)
 */
void Layer::drawLine(int contextId, const Brush& brush, const Point& from, const Point& to, qreal &distance)
{
	if(!brush.incremental()) {
		// Indirect brush: use a sublayer
		Layer *sl = getSubLayer(contextId, brush.blendingMode(), brush.opacity(1) * 255);

		Brush slb(brush);
		slb.setOpacity(1.0);
		slb.setOpacity2(brush.isOpacityVariable() ? 0.0 : 1.0);
		slb.setBlendingMode(1);

		if(brush.subpixel())
			sl->drawSoftLine(slb, from, to, distance);
		else
			sl->drawHardLine(slb, from, to, distance);
	} else {
		if(brush.subpixel())
			drawSoftLine(brush, from, to, distance);
		else
			drawHardLine(brush, from, to, distance);
	}
}

/**
 * This function is optimized for drawing with subpixel precision.
 * @param brush brush to draw the line with
 * @param from starting point
 * @param to ending point
 * @param distance distance from previous dab.
 */
void Layer::drawSoftLine(const Brush& brush, const Point& from, const Point& to, qreal &distance)
{
	const qreal spacing = brush.spacing()*brush.radius(from.pressure())/100.0;
	qreal x0 = from.x() + from.xFrac();
	qreal y0 = from.y() + from.yFrac();
	qreal p = from.pressure();
	qreal x1 = to.x() + to.xFrac();
	qreal y1 = to.y() + to.yFrac();
	const qreal dist = hypot(x1-x0,y1-y0);
	const qreal dx = (x1-x0)/dist;
	const qreal dy = (y1-y0)/dist;
	const qreal dp = (to.pressure()-from.pressure())/dist;
	// Skip the first dab.
	x0 += dx;
	y0 += dy;
	p += dp;
	for(qreal i=0;i<dist-0.5;++i) {
		if(++distance > spacing) {
			directDab(brush, Point(QPointF(x0,y0),qBound(0.0,p,1.0)));
			distance = 0;
		}
		x0 += dx;
		y0 += dy;
		p += dp;
	}
}

/**
 * This line drawing function is optimized for drawing with no subpixel
 * precision.
 * The last point is not drawn, so successive lines can be drawn blotches.
 */
void Layer::drawHardLine(const Brush& brush, const Point& from, const Point& to, qreal &distance) {
	const qreal dp = (to.pressure()-from.pressure()) / hypot(to.x()-from.x(), to.y()-from.y());

	const int spacing = brush.spacing()*brush.radius(from.pressure())/100;

	Point point = from;
	int &x0 = point.rx();
	int &y0 = point.ry();
	qreal &p = point.rpressure();
	int x1 = to.x();
	int y1 = to.y();
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

	if (dx > dy) {
		int fraction = dy - (dx >> 1);
		while (x0 != x1) {
			if (fraction >= 0) {
				y0 += stepy;
				fraction -= dx;
			}
			x0 += stepx;
			fraction += dy;
			if(++distance > spacing) {
				directDab(brush, point);
				distance = 0;
			}
			p += dp;
		}
	} else {
		int fraction = dx - (dy >> 1);
		while (y0 != y1) {
			if (fraction >= 0) {
				x0 += stepx;
				fraction -= dy;
			}
			y0 += stepy;
			fraction += dx;
			if(++distance > spacing) {
				directDab(brush, point);
				distance = 0;
			}
			p += dp;
		}
	}
}

/**
 * Apply a single dab of the brush to the layer
 * @param brush brush to use
 * @parma point where to dab. May be outside the image.
 */
void Layer::directDab(const Brush& brush, const Point& point)
{
	const int dia = brush.diameter(point.pressure())+1; // space for subpixels
	const int top = point.y() - brush.radius(point.pressure());
	const int left = point.x() - brush.radius(point.pressure());
	const int bottom = qMin(top + dia, height_);
	const int right = qMin(left + dia, width_);
	if(left+dia<=0 || top+dia<=0 || left>=width_ || top>=height_)
		return;

	// Render the brush
	BrushMask bm = brush.subpixel()?brush.render_subsampled(point.xFrac(), point.yFrac(), point.pressure()):brush.render(point.pressure());
	const int realdia = bm.diameter();
	const uchar *values = bm.data();
	QColor color = brush.color(point.pressure());

	// A single dab can (and often does) span multiple tiles.
	int y = top<0?0:top;
	int yb = top<0?-top:0; // y in relation to brush origin
	const int x0 = left<0?0:left;
	const int xb0 = left<0?-left:0;
	while(y<bottom) {
		const int yindex = y / Tile::SIZE;
		const int yt = y - yindex * Tile::SIZE;
		const int hb = yt+realdia-yb < Tile::SIZE ? realdia-yb : Tile::SIZE-yt;
		int x = x0;
		int xb = xb0; // x in relation to brush origin
		while(x<right) {
			const int xindex = x / Tile::SIZE;
			const int xt = x - xindex * Tile::SIZE;
			const int wb = xt+realdia-xb < Tile::SIZE ? realdia-xb : Tile::SIZE-xt;
			const int i = _xtiles * yindex + xindex;
			if(_tiles[i]==0)
				_tiles[i] = new Tile(xindex, yindex);
			_tiles[i]->composite(
					brush.blendingMode(),
					values + yb * realdia + xb,
					color,
					xt, yt,
					wb, hb,
					realdia-wb
					);

			x = (xindex+1) * Tile::SIZE;
			xb = xb + wb;
		}
		y = (yindex+1) * Tile::SIZE;
		yb = yb + hb;
	}
	if(owner_ && visible())
		owner_->markDirty(QRect(left, top, right-left, bottom-top));

}

/**
 * @param layer the layer that will be merged to this
 */
void Layer::merge(const Layer *layer)
{
	Q_ASSERT(layer->_xtiles == _xtiles);
	Q_ASSERT(layer->_ytiles == _ytiles);

	const bool md = owner_ && visible();

	for(int y=0;y<layer->_ytiles;++y) {
		for(int x=0;x<layer->_xtiles;++x) {
			const int index = _xtiles*y + x;

				if(layer->_tiles[index]==0)
				continue;

			if(_tiles[index]==0)
				_tiles[index] = new Tile(x, y);

			_tiles[index]->merge(layer->_tiles[index], layer->_opacity, layer->blendmode());
			if(md)
				owner_->markDirty(x, y);
		}
	}
}

void Layer::fillChecker(const QColor& dark, const QColor& light)
{
	for(int i=0;i<_xtiles*_ytiles;++i)
		_tiles[i]->fillChecker(dark, light);
	if(owner_ && visible())
		owner_->markDirty();
}

void Layer::fillColor(const QColor& color)
{
	for(int i=0;i<_xtiles*_ytiles;++i)
		_tiles[i]->fillColor(color);
	if(owner_ && visible())
		owner_->markDirty();
}

/**
 * Free all tiles that are completely transparent
 */
void Layer::optimize()
{
	for(int i=0;i<_xtiles*_ytiles;++i) {
		if(_tiles[i] && _tiles[i]->isBlank()) {
			delete _tiles[i];
			_tiles[i] = 0;
		}
	}
	// TODO delete unused sublayers
}

/**
 * @brief Get or create a new sublayer
 *
 * Sublayers are temporary layers used for indirect drawing.
 *
 * @param id sublayer ID (unique to parent layer only)
 * @param opacity layer opacity (set when creating the layer)
 * @return sublayer
 */
Layer *Layer::getSubLayer(int id, int blendmode, uchar opacity)
{
	// See if the sublayer exists already
	foreach(Layer *sl, _sublayers)
		if(sl->id() == id) {
			if(sl->hidden()) {
				// Hidden, reset properties
				sl->_hidden = false;
				sl->_opacity = opacity;
				sl->_blend = blendmode;
			}
			return sl;
		}

	// Okay, try recycling a sublayer
	foreach(Layer *sl, _sublayers) {
		if(sl->hidden()) {
			// Set these flags directly to avoid markDirty call.
			// We know the layer is invisible at this point
			sl->_hidden = false;
			sl->id_ = id;
			sl->_opacity = opacity;
			sl->_blend = blendmode;
			return sl;
		}
	}

	// No available sublayers, create a new one
	Layer *sl = new Layer(owner_, id, QSize(width_, height_));
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
			merge(sl);
			sl->_hidden = true;

			for(int i=0;i<_xtiles*_ytiles;++i) {
				delete sl->_tiles[i];
				sl->_tiles[i] = 0;
			}

			return;
		}
	}
}

}
