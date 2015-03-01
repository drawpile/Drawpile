/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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

#include "layerlist.h"
#include "core/layer.h"

#include <QDebug>
#include <QStringList>
#include <QBuffer>
#include <QImage>
#include <QRegularExpression>

namespace net {

LayerListModel::LayerListModel(QObject *parent)
	: QAbstractListModel(parent)
{
}
	
int LayerListModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return _items.size();
}

QVariant LayerListModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && index.row() >= 0 && index.row() <= _items.size()) {
		if(role == Qt::DisplayRole) {
			return QVariant::fromValue(_items.at(index.row()));
		} else if(role == Qt::EditRole) {
			// Edit role is for renaming the layer
			return _items.at(index.row()).title;
		}
	}
	return QVariant();
}

Qt::ItemFlags LayerListModel::flags(const QModelIndex& index) const
{
	if(!index.isValid())
		return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEnabled;

	return Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

Qt::DropActions LayerListModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

QStringList LayerListModel::mimeTypes() const {
		return QStringList() << "application/x-qt-image";
}

QMimeData *LayerListModel::mimeData(const QModelIndexList& indexes) const
{
	return new LayerMimeData(this, indexes[0].data().value<LayerListItem>().id);
}

bool LayerListModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
	Q_UNUSED(action);
	Q_UNUSED(column);
	Q_UNUSED(parent);

	const LayerMimeData *ldata = qobject_cast<const LayerMimeData*>(data);
	if(ldata && ldata->source() == this) {
		// note: if row is -1, the item was dropped on the parent element, which in the
		// case of the list view means the empty area below the items.
		handleMoveLayer(indexOf(ldata->layerId()), row<0 ? _items.count() : row);
	} else {
		// TODO support new layer drops
		qWarning() << "External layer drag&drop not supported";
	}
	return false;
}

void LayerListModel::handleMoveLayer(int oldIdx, int newIdx)
{
	// Need at least two layers for this to make sense
	const int count = _items.count();
	if(count < 2)
		return;

	QList<uint16_t> layers;
	layers.reserve(count);
	foreach(const LayerListItem &li, _items)
		layers.append(li.id);

	if(newIdx>oldIdx)
		--newIdx;

	layers.move(oldIdx, newIdx);

	// Layers are shown topmost first in the list but
	// are sent bottom first in the protocol.
	for(int i=0;i<count/2;++i)
		layers.swap(i,count-(1+i));

	emit layerOrderChanged(layers);
}

int LayerListModel::indexOf(int id) const
{
	for(int i=0;i<_items.size();++i)
		if(_items.at(i).id == id)
			return i;
	return -1;
}

QModelIndex LayerListModel::layerIndex(int id)
{
	int i = indexOf(id);
	if(i>=0)
		return index(i);
	return QModelIndex();
}

void LayerListModel::createLayer(int id, const QString &title)
{
	beginInsertRows(QModelIndex(),0,0);
	_items.prepend(LayerListItem(id, title));
	endInsertRows();
	emit layerCreated(_items.count()==1);
}

void LayerListModel::copyLayer(int sourceId, int id, const QString &title)
{
	// Find source layer
	int pos=-1;
	for(int i=0;i<_items.size();++i) {
		if(_items.at(i).id == sourceId) {
			pos = i;
			break;
		}
	}
	Q_ASSERT(pos>=0);
	if(pos<0)
		return;

	beginInsertRows(QModelIndex(),pos,pos);
	_items.insert(pos, LayerListItem(id, title));
	endInsertRows();
	emit layerCreated(false);
}

void LayerListModel::deleteLayer(int id)
{
	int row = indexOf(id);
	Q_ASSERT(row>=0);
	beginRemoveRows(QModelIndex(), row, row);
	_items.remove(row);
	endRemoveRows();
	emit layerDeleted(id, row+1);
}

void LayerListModel::clear()
{
	beginRemoveRows(QModelIndex(), 0, _items.size());
	_items.clear();
	endRemoveRows();
}

void LayerListModel::changeLayer(int id, float opacity, int blend)
{
	int row = indexOf(id);
	Q_ASSERT(row>=0);
	LayerListItem &item = _items[row];
	item.opacity = opacity;
	item.blend = blend;
	const QModelIndex qmi = index(row);
	emit dataChanged(qmi, qmi);
}

void LayerListModel::retitleLayer(int id, const QString &title)
{
	int row = indexOf(id);
	Q_ASSERT(row>=0);
	LayerListItem &item = _items[row];
	item.title = title;
	const QModelIndex qmi = index(row);
	emit dataChanged(qmi, qmi);
}

void LayerListModel::setLayerHidden(int id, bool hidden)
{
	int row = indexOf(id);
	Q_ASSERT(row>=0);
	LayerListItem &item = _items[row];
	item.hidden = hidden;
	const QModelIndex qmi = index(row);
	emit dataChanged(qmi, qmi);
}

void LayerListModel::updateLayerAcl(int id, bool locked, QList<uint8_t> exclusive)
{
	int row = indexOf(id);
	Q_ASSERT(row>=0);
	LayerListItem &item = _items[row];
	item.locked = locked;
	item.exclusive = exclusive;
	const QModelIndex qmi = index(row);
	emit dataChanged(qmi, qmi);
}

void LayerListModel::unlockAll()
{
	for(int i=0;i<_items.count();++i) {
		_items[i].locked = false;
		_items[i].exclusive.clear();
	}
	emit dataChanged(index(0), index(_items.count()));
}

void LayerListModel::reorderLayers(QList<uint16_t> neworder)
{
	QVector<LayerListItem> newitems;
	for(int j=neworder.size()-1;j>=0;--j) {
		const uint16_t id=neworder[j];
		for(int i=0;i<_items.size();++i) {
			if(_items[i].id == id) {
				newitems << _items[i];
				break;
			}
		}
	}
	_items = newitems;
	emit dataChanged(index(0), index(_items.size()));
	emit layersReordered();
}

void LayerListModel::setLayers(const QVector<LayerListItem> &items)
{
	beginResetModel();
	_items = items;
	endResetModel();
}

const paintcore::Layer *LayerListModel::getLayerData(int id) const
{
	if(_getlayerfn)
		return _getlayerfn(id);
	return nullptr;
}

void LayerListModel::previewOpacityChange(int id, float opacity)
{
	emit layerOpacityPreview(id, opacity);
}

QStringList LayerMimeData::formats() const
{
	return QStringList() << "application/x-qt-image";
}

QVariant LayerMimeData::retrieveData(const QString &mimeType, QVariant::Type type) const
{
	Q_UNUSED(mimeType);
	if(type==QVariant::Image) {
		const paintcore::Layer *layer = _source->getLayerData(_id);
		if(layer)
			return layer->toCroppedImage(0, 0);
	}

	return QVariant();
}

int LayerListModel::getAvailableLayerId() const
{
	const int prefix = _myId << 8;
	QList<int> takenIds;
	for(const LayerListItem &item : _items) {
		if((item.id & 0xff00) == prefix)
			takenIds.append(item.id);
	}

	for(int i=0;i<256;++i) {
		int id = prefix | i;
		if(!takenIds.contains(id))
			return id;
	}

	return 0;
}

QString LayerListModel::getAvailableLayerName(QString basename) const
{
	int tries=0;
	// See if the base name ends with a number. If so, remove it
	// and start iteration from there.
	QRegularExpression re("(\\d+)$");
	QRegularExpressionMatch m = re.match(basename);
	if(m.hasMatch()) {
		tries = m.captured(1).toInt();
		if(tries<0 || tries>254)
			tries = 0;
		else
			tries = tries + 1;

		basename = basename.mid(0, m.capturedStart()).trimmed();
	}

	// Find first unused name
	while(tries < 255) {
		QString name = basename;
		if(tries>0)
			name = name + " " + QString::number(tries);

		bool nameOk = true;
		for(int l=0;l<_items.size();++l) {
			const net::LayerListItem &layer = _items.at(l);
			if(layer.title.compare(name, Qt::CaseInsensitive)==0) {
				nameOk=false;
				break;
			}
		}
		if(nameOk)
			return name;

		++tries;
	}

	// Tried too many times without finding a name
	return QString();
}

}
