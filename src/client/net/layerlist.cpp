/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#include <QStringList>
#include "layerlist.h"
#include "layermimedata.h"

namespace net {

LayerListModel::LayerListModel(QObject *parent)
	: QAbstractListModel(parent)
{
}
	
int LayerListModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return _items.size() + 1;
}

QVariant LayerListModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && index.row() >= 0 && index.row() <= _items.size()) {
		if(role == Qt::DisplayRole) {
			// Row 0 is the special "add new layer" button row
			if(index.row()==0)
				return tr("New layer");
			else
				return QVariant::fromValue(_items.at(index.row()-1));
		} else if(role == Qt::EditRole) {
			// Edit role is for renaming the layer
			return _items.at(index.row()-1).title;
		}
	}
	return QVariant();
}

Qt::ItemFlags LayerListModel::flags(const QModelIndex& index) const
{
	if(!index.isValid())
		return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEnabled;

	const Qt::ItemFlags flags = Qt::ItemIsEnabled;
	if(index.row()==0) // Row 0 is the "add new layer" special item
		return flags;
	else
		return flags | Qt::ItemIsDragEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

Qt::DropActions LayerListModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

QStringList LayerListModel::mimeTypes() const {
        return QStringList() << "image/png";
}

QMimeData *LayerListModel::mimeData(const QModelIndexList& indexes) const
{
	return new LayerMimeData(indexes[0].data().value<LayerListItem>().id);
}

bool LayerListModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
	const LayerMimeData *ldata = qobject_cast<const LayerMimeData*>(data);
	if(ldata) {
		emit moveLayer(indexOf(ldata->layerId()), qMax(0, row-1));
	} else {
		qWarning() << "External layer drag&drop not supported";
	}
	return false;
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
		return index(i+1);
	return QModelIndex();
}

void LayerListModel::createLayer(int id, const QString &title)
{
	beginInsertRows(QModelIndex(),1,1);
	_items.prepend(LayerListItem(id, title));
	endInsertRows();

}

void LayerListModel::deleteLayer(int id)
{
	int row = indexOf(id);
	Q_ASSERT(row>=0);
	beginRemoveRows(QModelIndex(), row, row);
	_items.remove(row);
	endRemoveRows();
}

void LayerListModel::clear()
{
	beginRemoveRows(QModelIndex(), 1, _items.size());
	_items.clear();
	endRemoveRows();
}

void LayerListModel::changeLayer(int id, float opacity, const QString &title)
{
	int row = indexOf(id);
	Q_ASSERT(row>=0);
	LayerListItem &item = _items[row];
	item.opacity = opacity;
	item.title = title;
	const QModelIndex qmi = createIndex(row+1, 0);
	emit dataChanged(qmi, qmi);
}

void LayerListModel::updateLayerAcl(int id, bool locked, QList<uint8_t> exclusive)
{
	int row = indexOf(id);
	Q_ASSERT(row>=0);
	LayerListItem &item = _items[row];
	item.locked = locked;
	item.exclusive = exclusive;
	const QModelIndex qmi = createIndex(row+1, 0);
	emit dataChanged(qmi, qmi);
}

void LayerListModel::unlockAll()
{
	for(int i=0;i<_items.count();++i) {
		_items[i].locked = false;
		_items[i].exclusive.clear();
	}
	emit dataChanged(index(1), index(_items.count()));
}

void LayerListModel::reorderLayers(QList<uint8_t> neworder)
{
	QVector<LayerListItem> newitems;
	for(int j=neworder.size()-1;j>=0;--j) {
		const uint8_t id=neworder[j];
		for(int i=0;i<_items.size();++i) {
			if(_items[i].id == id) {
				newitems << _items[i];
				break;
			}
		}
	}
	_items = newitems;
	emit dataChanged(index(1,0), index(_items.size(),0));
}

}
