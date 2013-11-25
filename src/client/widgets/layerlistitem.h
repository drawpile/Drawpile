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
	LayerListItem() : id(0), title(""), opacity(1.0), hidden(false), locked(false) {}
	LayerListItem(int id_, const QString &title_, float opacity_=1.0, bool hidden_=false)
		: id(id_), title(title_), opacity(opacity_), hidden(hidden_), locked(false)
		{}

	//! Layer ID
	int id;
	
	//! Layer title
	QString title;
	
	//! Layer opacity
	float opacity;
	
	//! Layer hidden flag (local only)
	bool hidden;

	//! General layer lock
	bool locked;

	//! Exclusive access to these users
	QList<uint8_t> exclusive;

	bool isLockedFor(int userid) const { return locked || !(exclusive.isEmpty() || exclusive.contains(userid)); }
};

class LayerListModel : public QAbstractListModel {
	Q_OBJECT
public:
	LayerListModel(QObject *parent=0);
	
	int rowCount(const QModelIndex &parent=QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const;
	Qt::ItemFlags flags(const QModelIndex& index) const;
	Qt::DropActions supportedDropActions() const;
	QStringList mimeTypes() const;
	QMimeData *mimeData(const QModelIndexList& indexes) const;
	bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);

	QModelIndex layerIndex(int id);
	
	void clear();
	void createLayer(int id, const QString &title);
	void deleteLayer(int id);
	void changeLayer(int id, float opacity, const QString &title);
	void reorderLayers(QList<uint8_t> neworder);
	void updateLayerAcl(int id, bool locked, QList<uint8_t> exclusive);
	void unlockAll();
	
signals:
	void moveLayer(int idx, int afterIdx);

private:
	int indexOf(int id) const;
	
	QVector<LayerListItem> _items;
};

}

Q_DECLARE_METATYPE(widgets::LayerListItem);

#endif
