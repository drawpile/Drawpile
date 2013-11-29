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
#ifndef DP_NET_LAYERLIST_H
#define DP_NET_LAYERLIST_H

#include <QAbstractListModel>
#include <QMimeData>
#include <QVector>

namespace net {

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
	void changeLayer(int id, float opacity);
	void retitleLayer(int id, const QString &title);
	void setLayerHidden(int id, bool hidden);
	void reorderLayers(QList<uint8_t> neworder);
	void updateLayerAcl(int id, bool locked, QList<uint8_t> exclusive);
	void unlockAll();
	
signals:
	void layerCreated(bool wasfirst);
	void layerDeleted(int id, int idx);
	void layersReordered();

	//! Emitted when layers are manually reordered
	void layerOrderChanged(const QList<uint8_t> neworder);

private:
	void handleMoveLayer(int idx, int afterIdx);

	int indexOf(int id) const;
	
	QVector<LayerListItem> _items;
};

/**
 * A specialization of QMimeData for passing layers around inside
 * the application.
 */
class LayerMimeData : public QMimeData
{
	Q_OBJECT
	public:
		LayerMimeData(int id) : QMimeData(), _id(id) {}

		int layerId() const { return _id; }

		QStringList formats() const;

	private:
		int _id;
};

}

Q_DECLARE_METATYPE(net::LayerListItem)

#endif

