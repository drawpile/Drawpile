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
#include <QPixmap>
#include <QPainter>
#include <QMimeData>

#include "layer.h"
#include "layerstack.h"
#include "tile.h"
#include "rasterop.h"

namespace dpcore {

LayerStack::LayerStack(QObject *parent)
	: QObject(parent), width_(-1), height_(-1)
{
}

LayerStack::~LayerStack()
{
	foreach(Layer *l, layers_)
		delete l;
	delete [] cache_;
}

/**
 * If not already initialized, this is called automatically when the
 * first layer is added.
 * @param size the size of the layers (each layer must be this big)
 */
void LayerStack::init(const QSize& size)
{
	Q_ASSERT(!size.isEmpty());
	width_ = size.width();
	height_ = size.height();
	xtiles_ = width_ / Tile::SIZE + ((width_ % Tile::SIZE)>0);
	ytiles_ = height_ / Tile::SIZE + ((height_ % Tile::SIZE)>0);
	cache_ = new QPixmap[xtiles_*ytiles_];
}

#if 0
Layer *LayerStack::addLayer(int id, const QString& name, const QImage& image, const QPoint& offset)
{
	if(width_ < 0)
		init(image.size());
	
	qDebug() << "Adding layer " << image.size() << "with offset" << offset << "(image size" << width_ << height_ << ")";

	Layer *nl = new Layer(this, id, name, image, offset, QSize(width_, height_));
	nl->optimize();
	layers_.append(nl);
	return nl;
}
#endif

/**
 * @param id layer ID
 * @param name name of the new layer
 * @param color fill color
 */
Layer *LayerStack::addLayer(int id, const QString& name, const QColor& color)
{
	Q_ASSERT(width_>0 && height_>0);

	Layer *nl = new Layer(this, id, name, color, QSize(width_, height_));
	layers_.append(nl);
	if(color.alpha() > 0)
		markDirty();
	return nl;
}

/**
 * @param id layer ID
 * @return true if layer was found and deleted
 */
bool LayerStack::deleteLayer(int id)
{
	for(int i=0;i<layers_.size();++i) {
		if(layers_.at(i)->id() == id) {
			delete layers_.takeAt(i);
			markDirty();
			// TODO room for optimization: mark only nontransparent tiles as dirty
			return true;
		}
	}
	return false;
}

/**
 * @param neworder list of layer IDs in the new order
 */
void LayerStack::reorderLayers(const QList<int> &neworder)
{
	Q_ASSERT(neworder.size() == layers_.size());
	QList<Layer*> newstack;
	newstack.reserve(layers_.size());
	foreach(int id, neworder) {
		Layer *l = 0;
		for(int i=0;i<layers_.size();++i) {
			if(layers_.at(i)->id() == id) {
				l=layers_.takeAt(i);
				break;
			}
		}
		Q_ASSERT(l);
		newstack.append(l);
	}
	layers_ = newstack;
	markDirty();
}

/**
 * @param id id of the layer that will be merged
 */
void LayerStack::mergeLayerDown(int id) {
	const Layer *top;
	Layer *btm=0;
	for(int i=0;i<layers_.size();++i) {
		if(layers_[i]->id() == id) {
			top = layers_[i];
			if(i>0)
				btm = layers_[i-1];
			break;
		}
	}
	if(btm==0)
		qWarning() << "Tried to merge bottom-most layer";
	else
		btm->merge(0,0, top);
}

Layer *LayerStack::getLayerByIndex(int index)
{
	return layers_.at(index);
}

const Layer *LayerStack::getLayerByIndex(int index) const
{
	return layers_.at(index);
}

Layer *LayerStack::getLayer(int id)
{
	// Since the expected number of layers is always fairly low,
	// we can get away with a simple linear search. (Note that IDs
	// may appear in random order due to layers being moved around.)
	foreach(Layer *l, layers_)
		if(l->id() == id)
			return l;
	return 0;
}

/**
 * @param id layer id
 * @return layer index. Returns a negative index if layer is not found
 */
int LayerStack::indexOf(int id) const
{
	for(int i=0;i<layers_.size();++i)
		if(layers_.at(i)->id() == id)
			return i;
	return -1;
}

/**
 * Paint a view of the layer stack. The layers are composited
 * together according to their options.
 * @param rect area of image to paint
 * @param painter painter to use
 */
void LayerStack::paint(const QRectF& rect, QPainter *painter)
{
	const int top = qMax(int(rect.top()), 0);
	const int left = qMax(int(rect.left()), 0);
	const int right = Tile::roundUp(qMin(int(rect.right()), width_));
	const int bottom = Tile::roundUp(qMin(int(rect.bottom()), height_));

	// TODO use a single big pixmap as a cache instead?
	painter->save();
	painter->setClipRect(rect);
	for(int y=top;y<bottom;y+=Tile::SIZE) {
		const int yindex = (y/Tile::SIZE);
		for(int x=left;x<right;x+=Tile::SIZE) {
			const int xindex = x/Tile::SIZE;
			int i = xtiles_*yindex + xindex;
			if(cache_[i].isNull())
				updateCache(xindex, yindex);
			painter->drawPixmap(QPoint(xindex*Tile::SIZE, yindex*Tile::SIZE),
					cache_[i]);
		}
	}
	painter->restore();

}

QColor LayerStack::colorAt(int x, int y) const
{
	// TODO merge
	if(layers_.isEmpty())
		return QColor();

	return layers_.at(0)->colorAt(x, y);
}

QImage LayerStack::toFlatImage() const
{
	// FIXME: this won't work if layer 0 is hidden or has opacity < 100%
	// TODO use a new empty lauyer instead?
	Layer *scratch = Layer::scratchCopy(layers_[0]);

	for(int i=1;i<layers_.size();++i)
		scratch->merge(0,0, layers_[i]);

	QImage img = scratch->toImage();
	delete scratch;
	return img;
}

// Flatten a single tile
void LayerStack::flattenTile(quint32 *data, int xindex, int yindex)
{
	// Start out with a checkerboard pattern to denote transparency
	Tile::fillChecker(data, QColor(128,128,128), Qt::white);

	// Composite visible layers
	foreach(const Layer *l, layers_) {
		if(l->visible()) {
			const Tile *tile = l->tile(xindex, yindex);
			if(tile) {
				compositePixels(1, data, tile->data(),
						Tile::SIZE*Tile::SIZE, l->opacity());
			}
		}
	}
}

// Update the paint cache. The layers are composited together
// according to their blend mode and opacity options.
void LayerStack::updateCache(int xindex, int yindex)
{
	quint32 data[Tile::SIZE*Tile::SIZE];
	flattenTile(data, xindex, yindex);
	cache_[yindex*xtiles_+xindex] = QPixmap::fromImage(
			QImage(reinterpret_cast<const uchar*>(data), Tile::SIZE, Tile::SIZE,
				QImage::Format_RGB32)
			);

	// This is needed for Windows, since QPixmap shares the memory.
	// On other systems, QPixmap data is stored elsewhere (i.e. in
	// display server memory)
	cache_[yindex*xtiles_+xindex].detach();

}

void LayerStack::markDirty(const QRect &area)
{
	int tx0 = area.left() / Tile::SIZE;
	int tx1 = area.right() / Tile::SIZE;
	int ty0 = area.top() / Tile::SIZE;
	int ty1 = area.bottom() / Tile::SIZE;
	
	for(;ty0<=ty1;++ty0) {
		for(int tx=tx0;tx<=tx1;++tx) {
			cache_[ty0*xtiles_ + tx] = QPixmap();
		}
	}
	emit areaChanged(area);
}

void LayerStack::markDirty()
{
	for(int i=0;i<xtiles_*ytiles_;++i)
		cache_[i] = QPixmap();
	emit areaChanged(QRect(0, 0, width_, height_));
}

}
