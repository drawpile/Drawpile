/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2009 Calle Laakkonen

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
#include "layermimedata.h"
#include "tile.h"
#include "rasterop.h"

namespace dpcore {

LayerStack::LayerStack(QObject *parent) :
	QAbstractListModel(parent), width_(-1), height_(-1), lastid_(0)
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
	Q_ASSERT(!size.isNull());
	width_ = size.width();
	height_ = size.height();
	xtiles_ = width_ / Tile::SIZE + ((width_ % Tile::SIZE)>0);
	ytiles_ = height_ / Tile::SIZE + ((height_ % Tile::SIZE)>0);
	cache_ = new QPixmap[xtiles_*ytiles_];
}

Layer *LayerStack::addLayer(const QString& name, const QImage& image, const QPoint& offset)
{
	if(width_ < 0)
		init(image.size());
	
	qDebug() << "Adding layer " << image.size() << "with offset" << offset << "(image size" << width_ << height_ << ")";

	Layer *nl = new Layer(this, lastid_++, name, image, offset, QSize(width_, height_));
	nl->optimize();
	beginInsertRows(QModelIndex(),1,1);
	layers_.append(nl);
	endInsertRows();
	return nl;
}

/**
 * @param name name of the new layer
 * @param color fill color
 * @param size layer size (may be omitted if stack is already initialized)
 */
Layer *LayerStack::addLayer(const QString& name, const QColor& color, const QSize& size)
{
	if(width_ < 0)
		init(size);

	Layer *nl = new Layer(this, lastid_++, name, color, QSize(width_, height_));
	beginInsertRows(QModelIndex(),1,1);
	layers_.append(nl);
	endInsertRows();
	return nl;
}

/**
 * @param name name of the new layer
 * @param size layer size (may be omitted if stack is already initialized)
 */
Layer *LayerStack::addLayer(const QString& name, const QSize& size)
{
	if(width_ < 0)
		init(size);

	Layer *nl = new Layer(this, lastid_++, name, QSize(width_, height_));
	beginInsertRows(QModelIndex(),1,1);
	layers_.append(nl);
	endInsertRows();
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
			// Invalidate cache
			Layer *l = layers_.at(i);
			for(int j=0;j<xtiles_*ytiles_;++j) {
				if(l->tile(j))
					cache_[j] = QPixmap();
			}
			// Remove the layer
			int row = layers() - i;
			beginRemoveRows(QModelIndex(), row, row);
			layers_.removeAt(i);
			endRemoveRows();
			return true;
		}
	}
	return false;
}

/**
 * @param src ID of the layer to move
 * @param dest ID of the layer under which src will be moved. -1 is a virtual layer above the topmost one
 */
void LayerStack::moveLayer(int src, int dest)
{
	int from = id2index(src);
	int to = dest >= 0 ? id2index(dest) : 0;
	layers_.move(from, to);
	if(layers_.at(from)->visible())
		markDirty();

	QModelIndex old = index(layers() - from, 0);
	QModelIndex newi = index(layers() - to, 0);

	// Signal views of the change
	if(from < to)
		qSwap(from, to);
	emit dataChanged(
			index(layers() - from, 0),
			index(layers() - to, 0)
			);

	emit layerMoved(old, newi);
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
		qWarning() << "SLOPPY: Tried to merge bottom-most layer";
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

bool LayerStack::isBottommost(const Layer *layer) const
{
	return layers_.first() == layer;
}

/**
 * @param id layer id
 * @return layer index. Returns a negative index if layer is not found
 */
int LayerStack::id2index(int id) const
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
	const int right = Tile::roundTo(qMin(int(rect.right()), width_));
	const int bottom = Tile::roundTo(qMin(int(rect.bottom()), height_));

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
	return layers_.at(0)->colorAt(x, y);
}

QImage LayerStack::toFlatImage() const
{
	// FIXME: this won't work if layer 0 is hidden or has opacity < 100%
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

void LayerStack::markDirty(int tilex, int tiley)
{
	cache_[tiley*xtiles_ + tilex] = QPixmap();
}

void LayerStack::markDirty()
{
	for(int i=0;i<xtiles_*ytiles_;++i)
		cache_[i] = QPixmap();
}

/**
 * This is called from the layers belonging to this stack when
 * changing.
 * Specifically, it is called when:
 * <ul>
 * <li>Layer opacity is changed</li>
 * <li>Layer name is changed</li>
 * </ul>
 * @param layer the layer which changed
 */
void LayerStack::layerChanged(const Layer* layer)
{
	int index=layers_.indexOf(const_cast<Layer*>(layer));
	Q_ASSERT(index >= 0);
	const QModelIndex qmi = createIndex(layers()-index, 0);
	emit dataChanged(qmi, qmi);
}

// Model related functions
QVariant LayerStack::data(const QModelIndex& index, int role) const
{
	// Always display one extra layer (for adding new layers)
	if(index.row() >= 0 && index.row() <= layers()) {
		// Display the layers in reverse order (topmost layer first)
		int row = layers() - index.row();
		if(role == Qt::DisplayRole) {
			if(index.row()==0)
				return "New";
			return QVariant::fromValue(layers_.at(row));
		} else if(role == Qt::EditRole) {
			return layers_.at(row)->name();
		}
	}
	return QVariant();
}

Qt::ItemFlags LayerStack::flags(const QModelIndex& index) const
{
	const Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;
	if(index.row()==0) // Row 0 is the "add new layer" special item
		return flags;
	else
		return flags | Qt::ItemIsDragEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

Qt::DropActions LayerStack::supportedDropActions() const
{
	// Support (internal) moving
	return Qt::MoveAction;
}

QStringList LayerStack::mimeTypes() const {
	return QStringList() << "image/png";
}

QMimeData *LayerStack::mimeData(const QModelIndexList& indexes) const
{
	return new LayerMimeData(layers_.at(layers() - indexes[0].row()));
}

bool LayerStack::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
	const LayerMimeData *ldata = qobject_cast<const LayerMimeData*>(data);
	if(ldata) {
		int from = ldata->layer()->id();
		int to;
		if(parent.row() <= 0)
			to = layers_.last()->id();
		else
			to = layers_.at(layers() - parent.row())->id();
		if(from<0) {
			qDebug() << "TODO: Cross-image layer DnD";
		} else {
			emit layerMoveRequest(from, to);

			return true;
		}
	} else {
		qDebug() << "TODO: Insert layer from external source";
	}
	return false;
}

int LayerStack::rowCount(const QModelIndex& parent) const
{
	if(parent.isValid())
		return 0;
	return layers() + 1;
}

}
