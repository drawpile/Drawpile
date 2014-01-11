/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2014 Calle Laakkonen

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
#include <QtConcurrent>

#include "layer.h"
#include "layerstack.h"
#include "tile.h"
#include "rasterop.h"

namespace paintcore {

LayerStack::LayerStack(QObject *parent)
	: QObject(parent), _width(0), _height(0)
{
}

LayerStack::~LayerStack()
{
	foreach(Layer *l, _layers)
		delete l;
}

void LayerStack::resize(int top, int right, int bottom, int left)
{
	int newtop = -top;
	int newleft = -left;
	int newright = _width + right;
	int newbottom = _height + bottom;
	if(newtop >= newbottom || newleft >= newright) {
		qWarning() << "Invalid resize: borders reversed";
		return;
	}
	_width = newright - newleft;
	_height = newbottom - newtop;

	_xtiles = Tile::roundTiles(_width);
	_ytiles = Tile::roundTiles(_height);
	_cache = QPixmap(_width, _height);
	_dirtytiles = QBitArray(_xtiles*_ytiles, true);

	foreach(Layer *l, _layers)
		l->resize(top, right, bottom, left);

	emit resized(left, top);
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
			_layers.at(i)->markOpaqueDirty();
			delete _layers.takeAt(i);
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
		btm->merge(top);
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

namespace {

struct UpdateTile {
	UpdateTile() : x(-1), y(-1) {}
	UpdateTile(int x_, int y_) : x(x_), y(y_) {}

	int x, y;
	quint32 data[Tile::LENGTH];
};

}

/**
 * Paint a view of the layer stack. The layers are composited
 * together according to their options.
 * @param rect area of image to paint
 * @param painter painter to use
 */
void LayerStack::paint(const QRectF& rect, QPainter *painter)
{
	if(_width<=0 || _height<=0)
		return;

	// Affected tile range
	const int tx0 = qBound(0, int(rect.left()) / Tile::SIZE, _xtiles-1);
	const int tx1 = qBound(tx0, int(rect.right()) / Tile::SIZE, _xtiles-1);
	const int ty0 = qBound(0, int(rect.top()) / Tile::SIZE, _ytiles-1);
	const int ty1 = qBound(ty0, int(rect.bottom()) / Tile::SIZE, _ytiles-1);

	// Gather list of tiles in need of updating
	QList<UpdateTile*> updates;

	for(int ty=ty0;ty<=ty1;++ty) {
		const int y = ty*_xtiles;
		for(int tx=tx0;tx<=tx1;++tx) {
			const int i = y+tx;
			if(_dirtytiles.testBit(i)) {
				updates.append(new UpdateTile(tx, ty));
				_dirtytiles.clearBit(i);
			}
		}
	}

	if(!updates.isEmpty()) {
		// Flatten tiles
		QtConcurrent::blockingMap(updates, [this](UpdateTile *t) {
			flattenTile(t->data, t->x, t->y);
		});

		// Repaint cache
		QPainter cache(&_cache);
		cache.setCompositionMode(QPainter::CompositionMode_Source);
		while(!updates.isEmpty()) {
			UpdateTile *ut = updates.takeLast();
			cache.drawImage(
				ut->x*Tile::SIZE,
				ut->y*Tile::SIZE,
				QImage(reinterpret_cast<const uchar*>(ut->data),
					Tile::SIZE, Tile::SIZE,
					QImage::Format_ARGB32
				)
			);
			delete ut;
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
		flat.merge(l);

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
			const Tile &tile = l->tile(xindex, yindex);
			if(l->sublayers().count()) {
				// Sublayers present, composite them first
				quint32 ldata[Tile::SIZE*Tile::SIZE];
				tile.copyTo(ldata);

				foreach(const Layer *sl, l->sublayers()) {
					if(sl->visible()) {
						const Tile &subtile = sl->tile(xindex, yindex);
						if(!subtile.isNull()) {
							compositePixels(sl->blendmode(), ldata, subtile.data(),
									Tile::SIZE*Tile::SIZE, sl->opacity());
						}
					}
				}

				// Composite merged tile
				compositePixels(l->blendmode(), data, ldata,
						Tile::SIZE*Tile::SIZE, l->opacity());
			} else if(!tile.isNull()) {
				// No sublayers, just this tile
				compositePixels(l->blendmode(), data, tile.data(),
						Tile::SIZE*Tile::SIZE, l->opacity());
			}
		}
	}
}

void LayerStack::markDirty(const QRect &area)
{
	if(_layers.isEmpty())
		return;
	int tx0 = qBound(0, area.left() / Tile::SIZE, _xtiles-1);
	int tx1 = qBound(tx0, area.right() / Tile::SIZE, _xtiles-1);
	int ty0 = qBound(0, area.top() / Tile::SIZE, _ytiles-1);
	int ty1 = qBound(ty0, area.bottom() / Tile::SIZE, _ytiles-1);
	
	for(;ty0<=ty1;++ty0) {
		for(int tx=tx0;tx<=tx1;++tx) {
			_dirtytiles.setBit(ty0*_xtiles + tx);
		}
	}
	_dirtyrect |= area;
}

void LayerStack::markDirty()
{
	if(_layers.isEmpty())
		return;
	_dirtytiles.fill(true);

	_dirtyrect = QRect(0, 0, _width, _height);
	notifyAreaChanged();
}

void LayerStack::markDirty(int x, int y)
{
	Q_ASSERT(x>=0 && x < _xtiles);
	Q_ASSERT(y>=0 && y < _ytiles);

	_dirtytiles.setBit(y*_xtiles + x);

	_dirtyrect |= QRect(x*Tile::SIZE, y*Tile::SIZE, Tile::SIZE, Tile::SIZE);
}

void LayerStack::notifyAreaChanged()
{
	if(!_dirtyrect.isEmpty()) {
		emit areaChanged(_dirtyrect);
		_dirtyrect = QRect();
	}
}

Savepoint::~Savepoint()
{
	while(!layers.isEmpty())
		delete layers.takeLast();
}

Savepoint *LayerStack::makeSavepoint()
{
	Savepoint *sp = new Savepoint;
	foreach(Layer *l, _layers) {
		l->optimize();
		sp->layers.append(new Layer(*l));
	}

	sp->width = _width;
	sp->height = _height;

	return sp;
}

void LayerStack::restoreSavepoint(const Savepoint *savepoint)
{
	while(!_layers.isEmpty())
		delete _layers.takeLast();
	foreach(const Layer *l, savepoint->layers)
		_layers.append(new Layer(*l));


	if(_width != savepoint->width || _height != savepoint->height) {
		_width = savepoint->width;
		_height = savepoint->height;
		_xtiles = Tile::roundTiles(_width);
		_ytiles = Tile::roundTiles(_height);
		_cache = QPixmap(_width, _height);
		_dirtytiles = QBitArray(_xtiles*_ytiles, true);
		emit resized(0, 0);
	} else {
		// TODO mark only changed tiles as dirty
		markDirty();
	}
}

}
