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
	: QObject(parent), _width(-1), _height(-1)
{
}

LayerStack::~LayerStack()
{
	foreach(Layer *l, _layers)
		delete l;
}

/**
 * If not already initialized, this is called automatically when the
 * first layer is added.
 * @param size the size of the layers (each layer must be this big)
 */
void LayerStack::init(const QSize& size)
{
	Q_ASSERT(!size.isEmpty());
	_width = size.width();
	_height = size.height();
	_xtiles = _width / Tile::SIZE + ((_width % Tile::SIZE)>0);
	_ytiles = _height / Tile::SIZE + ((_height % Tile::SIZE)>0);
	_cache = QPixmap(size);
	_dirtytiles = QBitArray(_xtiles*_ytiles, true);
}

/**
 * @param id layer ID
 * @param name name of the new layer
 * @param color fill color
 */
Layer *LayerStack::addLayer(int id, const QString& name, const QColor& color)
{
	Q_ASSERT(_width>0 && _height>0);

	Layer *nl = new Layer(this, id, name, color, QSize(_width, _height));
	_layers.append(nl);
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
	for(int i=0;i<_layers.size();++i) {
		if(_layers.at(i)->id() == id) {
			delete _layers.takeAt(i);
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
void LayerStack::reorderLayers(const QList<uint8_t> &neworder)
{
	Q_ASSERT(neworder.size() == _layers.size());
	QList<Layer*> newstack;
	newstack.reserve(_layers.size());
	foreach(int id, neworder) {
		Layer *l = 0;
		for(int i=0;i<_layers.size();++i) {
			if(_layers.at(i)->id() == id) {
				l=_layers.takeAt(i);
				break;
			}
		}
		Q_ASSERT(l);
		newstack.append(l);
	}
	_layers = newstack;
	markDirty();
}

/**
 * @param id id of the layer that will be merged
 */
void LayerStack::mergeLayerDown(int id) {
	const Layer *top;
	Layer *btm=0;
	for(int i=0;i<_layers.size();++i) {
		if(_layers[i]->id() == id) {
			top = _layers[i];
			if(i>0)
				btm = _layers[i-1];
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
	return _layers.at(index);
}

const Layer *LayerStack::getLayerByIndex(int index) const
{
	return _layers.at(index);
}

Layer *LayerStack::getLayer(int id)
{
	// Since the expected number of layers is always fairly low,
	// we can get away with a simple linear search. (Note that IDs
	// may appear in random order due to layers being moved around.)
	foreach(Layer *l, _layers)
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
	for(int i=0;i<_layers.size();++i)
		if(_layers.at(i)->id() == id)
			return i;
	return -1;
}

void LayerStack::setLayerHidden(int layerid, bool hide)
{
	Layer *l = getLayer(layerid);
	if(l) {
		l->setHidden(hide);
	} else {
		qWarning() << "Tried to set hidden flag of non-existent layer" << layerid;
	}
}

/**
 * Paint a view of the layer stack. The layers are composited
 * together according to their options.
 * @param rect area of image to paint
 * @param painter painter to use
 */
void LayerStack::paint(const QRectF& rect, QPainter *painter)
{
	// Refresh cache
	const int tx0 = qBound(0, int(rect.left() / Tile::SIZE), _xtiles-1);
	const int tx1 = qBound(0, int(rect.right() / Tile::SIZE), _xtiles-1);
	const int ty0 = qBound(0, int(rect.top() / Tile::SIZE), _ytiles-1);
	const int ty1 = qBound(0, int(rect.bottom() / Tile::SIZE), _ytiles-1);

	for(int ty=ty0;ty<=ty1;++ty) {
		const int y = ty*_xtiles;
		for(int tx=tx0;tx<=tx1;++tx) {
			const int i = y+tx;
			if(_dirtytiles.at(i)) {
				updateCache(tx,ty);
				_dirtytiles.clearBit(i);
			}
		}
	}

	// Paint the cached pixmap
	painter->drawPixmap(rect, _cache, rect);
}

QColor LayerStack::colorAt(int x, int y) const
{
	if(_layers.isEmpty())
		return QColor();

	// TODO some more efficient way of doing this
	quint32 tile[Tile::SIZE*Tile::SIZE];
	flattenTile(tile, x/Tile::SIZE, y/Tile::SIZE);
	quint32 c = tile[(y-Tile::roundDown(y)) * Tile::SIZE + (x-Tile::roundDown(x))];
	return QColor(c);
}

QImage LayerStack::toFlatImage() const
{
	Layer flat(0, 0, "", Qt::transparent, QSize(_width, _height));

	foreach(const Layer *l, _layers)
		flat.merge(0, 0, l);

	return flat.toImage();
}

// Flatten a single tile
void LayerStack::flattenTile(quint32 *data, int xindex, int yindex) const
{
	// Start out with a checkerboard pattern to denote transparency
	Tile::fillChecker(data, QColor(128,128,128), Qt::white);

	// Composite visible layers
	foreach(const Layer *l, _layers) {
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
	Q_ASSERT(xindex >= 0 && xindex < _xtiles);
	Q_ASSERT(yindex >= 0 && yindex < _ytiles);

	quint32 data[Tile::SIZE*Tile::SIZE];
	flattenTile(data, xindex, yindex);
	QPainter painter(&_cache);
	painter.setCompositionMode(QPainter::CompositionMode_Source);
	painter.drawImage(
		xindex*Tile::SIZE,
		yindex*Tile::SIZE,
		QImage(reinterpret_cast<const uchar*>(data),
			Tile::SIZE, Tile::SIZE,
			QImage::Format_ARGB32
		)
	);
}

void LayerStack::markDirty(const QRect &area)
{
	int tx0 = area.left() / Tile::SIZE;
	int tx1 = qMin(_width-1, area.right()) / Tile::SIZE;
	int ty0 = area.top() / Tile::SIZE;
	int ty1 = qMin(_height-1, area.bottom()) / Tile::SIZE;
	
	for(;ty0<=ty1;++ty0) {
		for(int tx=tx0;tx<=tx1;++tx) {
			_dirtytiles.setBit(ty0*_xtiles + tx);
		}
	}
	emit areaChanged(area);
}

void LayerStack::markDirty()
{
	_dirtytiles.fill(true);
	emit areaChanged(QRect(0, 0, _width, _height));
}

}
