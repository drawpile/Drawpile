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
#ifndef DP_LAYERLISTWIDGET_ITEM_H
#define DP_LAYERLISTWIDGET_ITEM_H

#include <QAbstractListModel>
#include <QVector>

namespace widgets {

struct LayerListItem {
	//! Layer ID
	int id;
	
	//! Layer title
	QString title;
	
	//! Layer opacity
	float opacity;
	
	//! Layer hidden flag (local only)
	bool hidden;
};

class LayerListModel : public QAbstractListModel {
	Q_OBJECT
public:
	LayerListModel(QObject *parent=0);
	
	int rowCount(const QModelIndex &parent=QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const;
	
public slots:
	void createLayer(int id, const QString &title);
	void deleteLayer(int id);
	void changeLayer(const LayerListItem &layer);
	void reorderLayers(QList<int> neworder);
	void setLayers(QVector<LayerListItem> items);

private:
	QVector<LayerListItem> _items;
};

}

Q_DECLARE_METATYPE(widgets::LayerListItem);

#endif
