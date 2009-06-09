/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

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

void Layer::init(LayerStack *owner, int id, const QString& name, const QSize& size)
{
	owner_ = owner;
	id_ = id;
	name_ = name;
	width_ = size.width();
	height_ = size.height();
	xtiles_ = width_ / Tile::SIZE + ((width_ % Tile::SIZE)>0);
	ytiles_ = height_ / Tile::SIZE + ((height_ % Tile::SIZE)>0);
	tiles_ = new Tile*[xtiles_ * ytiles_];
	opacity_ = 255;
}

/**
 * Construct a layer whose content is loaded from the provided image.
 * @param owner the stack to which this layer belongs to
 * @param id layer ID
 * @param image image on which the layer is based
 */
Layer::Layer(LayerStack *owner, int id, const QString& name, const QImage& image) {
	init(owner, id, name, image.size());
	for(int y=0;y<ytiles_;++y)
		for(int x=0;x<xtiles_;++x)
			tiles_[y*xtiles_+x] = new Tile(image, x, y);
}

/**
 * Construct a layer initialized to a solid color
 * @param owner the stack to which this layer belongs to
 * @param id layer ID
 * @param color layer color
 * @parma size layer size
 */
Layer::Layer(LayerStack *owner, int id, const QString& name, const QColor& color, const QSize& size) {
	init(owner, id, name, size);
	for(int y=0;y<ytiles_;++y)
		for(int x=0;x<xtiles_;++x)
			tiles_[y*xtiles_+x] = new Tile(color, x, y);
}

/**
 * Construct an empty (transparent) layer.
 * Memory for the layer tiles will be allocated on demand.
 * @param owner the stack to which this layer belongs to
 * @param id layer ID
 * @param size layer size
 */
Layer::Layer(LayerStack *owner, int id, const QString& name, const QSize& size)
{
	init(owner, id, name, size);
	for(int i=0;i<xtiles_*ytiles_;++i)
		tiles_[i] = 0;
}

Layer::~Layer() {
	for(int i=0;i<xtiles_*ytiles_;++i)
		delete tiles_[i];
	delete [] tiles_;
}

QImage Layer::toImage() const {
	QImage image(width_, height_, QImage::Format_ARGB32_Premultiplied);
	for(int i=0;i<xtiles_*ytiles_;++i) {
		if(tiles_[i])
			tiles_[i]->copyToImage(image);
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
	return QColor::fromRgb(
			tiles_[yindex*xtiles_ + xindex]->pixel(
				x-xindex*Tile::SIZE,
				y-yindex*Tile::SIZE
				));
}

/**
 * @param opacity
 */
void Layer::setOpacity(int opacity)
{
	Q_ASSERT(opacity>=0 && opacity<256);
	opacity_ = opacity;
	// TODO optimization: mark only nonempty tiles
	owner_->markDirty();
}

/**
 * Apply a single dab of the brush to the layer
 * @param brush brush to use
 * @parma point where to dab. May be outside the image.
 */
void Layer::dab(const Brush& brush, const Point& point)
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
			const int i = xtiles_ * yindex + xindex;
			if(tiles_[i]==0)
				tiles_[i] = new Tile(xindex, yindex);
			tiles_[i]->composite(
					brush.blendingMode(),
					values + yb * realdia + xb,
					color,
					xt, yt,
					wb, hb,
					realdia-wb
					);

			if(owner_ && visible())
				owner_->markDirty(xindex, yindex);

			x = (xindex+1) * Tile::SIZE;
			xb = xb + wb;
		}
		y = (yindex+1) * Tile::SIZE;
		yb = yb + hb;
	}
}

/**
 * Draw a line using either drawSoftLine or drawHardLine, depending on
 * the subpixel hint of the brush.
 */
void Layer::drawLine(const Brush& brush, const Point& from, const Point& to, qreal &distance)
{
	if(brush.subpixel())
		drawSoftLine(brush, from, to, distance);
	else
		drawHardLine(brush, from, to, distance);
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
			dab(brush, Point(QPointF(x0,y0),qBound(0.0,p,1.0)));
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
				dab(brush, point);
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
				dab(brush, point);
				distance = 0;
			}
			p += dp;
		}
	}
}

/**
 * @param tile x index offset
 * @param tile y index offset
 */
void Layer::merge(int x, int y, const Layer *layer)
{
	int myx = x;
	int myy = y;
	for(int i=0;i<layer->ytiles_&&myy<ytiles_;++i,myy++) {
		for(int j=0;j<layer->xtiles_;++j&&myx<xtiles_,myx++) {
			const int index = xtiles_*myy + myx;
			if(tiles_[index]==0)
				tiles_[index] = new Tile(myx, myy);
			tiles_[xtiles_*myy+myx]->merge(layer->tiles_[layer->xtiles_*i+j], opacity_);
			if(owner_ && visible())
				owner_->markDirty(myx, myy);
		}
		myx = x;
	}
}

void Layer::fillChecker(const QColor& dark, const QColor& light)
{
	for(int i=0;i<xtiles_*ytiles_;++i)
		tiles_[i]->fillChecker(dark, light);
}

}
