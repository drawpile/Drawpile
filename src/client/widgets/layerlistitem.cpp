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

#include "layerlistitem.h"

namespace widgets {

LayerListModel::LayerListModel(QObject *parent)
	: QAbstractListModel(parent)
{
}
	
int LayerListModel::rowCount(const QModelIndex &parent) const
{
	return _items.size() + 1;
}

QVariant LayerListModel::data(const QModelIndex &index, int role) const
{
	if(index.row() >= 0 && index.row() <= _items.size()) {
		// Display the layers in reverse order (topmost layer first)
		int row = _items.size() - index.row();
		if(role == Qt::DisplayRole) {
			if(index.row()==0)
					return "New";
			return QVariant::fromValue(_items.at(row));
		}/* else if(role == Qt::EditRole) {
				return layers_.at(row)->name();
		}*/
	}
	return QVariant();
}

void LayerListModel::createLayer(int id, const QString &title)
{
}

void LayerListModel::deleteLayer(int id)
{
}

void LayerListModel::changeLayer(const LayerListItem &layer)
{
}

void LayerListModel::reorderLayers(QList<int> neworder)
{
}

void LayerListModel::setLayers(QVector<LayerListItem> items)
{
}

}


