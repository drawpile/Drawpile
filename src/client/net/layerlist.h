/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#ifndef DP_NET_LAYERLIST_H
#define DP_NET_LAYERLIST_H

#include <QAbstractListModel>
#include <QMimeData>
#include <QVector>

#include <functional>

namespace paintcore {
	class Layer;
}

namespace net {

struct LayerListItem {
	LayerListItem() : id(0), title(QString()), opacity(1.0), blend(1), hidden(false), locked(false) {}
	LayerListItem(int id_, const QString &title_, float opacity_=1.0, int blend_=1, bool hidden_=false, bool locked_=false, const QList<uint8_t> &exclusive_=QList<uint8_t>())
		: id(id_), title(title_), opacity(opacity_), blend(blend_), hidden(hidden_), locked(locked_), exclusive(exclusive_)
		{}

	//! Layer ID
	int id;
	
	//! Layer title
	QString title;
	
	//! Layer opacity
	float opacity;
	
	//! Blending mode
	int blend;

	//! Layer hidden flag (local only)
	bool hidden;

	//! General layer lock
	bool locked;

	//! Exclusive access to these users
	QList<uint8_t> exclusive;

	bool isLockedFor(int userid) const { return locked || !(exclusive.isEmpty() || exclusive.contains(userid)); }
};

}

Q_DECLARE_TYPEINFO(net::LayerListItem, Q_MOVABLE_TYPE);

namespace net {

typedef std::function<const paintcore::Layer*(int id)> GetLayerFunction;

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
	void createLayer(int id, int index, const QString &title);
	void deleteLayer(int id);
	void changeLayer(int id, float opacity, int blend);
	void retitleLayer(int id, const QString &title);
	void setLayerHidden(int id, bool hidden);
	void reorderLayers(QList<uint16_t> neworder);
	void updateLayerAcl(int id, bool locked, QList<uint8_t> exclusive);
	void unlockAll();
	
	QVector<LayerListItem> getLayers() const { return _items; }
	void setLayers(const QVector<LayerListItem> &items);

	void previewOpacityChange(int id, float opacity);

	void setLayerGetter(GetLayerFunction fn) { _getlayerfn = fn; }
	const paintcore::Layer *getLayerData(int id) const;

	void setMyId(int id) { _myId = id; }

	/**
	 * @brief Find a free layer ID
	 * @return layer ID or 0 if all are taken
	 */
	int getAvailableLayerId() const;

	/**
	 * @brief Find a unique name for a layer
	 * @param basename
	 * @return unique name
	 */
	QString getAvailableLayerName(QString basename) const;

signals:
	void layerCreated(bool wasfirst);
	void layerDeleted(int id, int idx);
	void layersReordered();

	//! Emitted when layers are manually reordered
	void layerOrderChanged(const QList<uint16_t> neworder);

	//! Request local change of layer opacity for preview purpose
	void layerOpacityPreview(int id, float opacity);

private:
	void handleMoveLayer(int idx, int afterIdx);

	int indexOf(int id) const;
	
	QVector<LayerListItem> _items;
	GetLayerFunction _getlayerfn;
	int _myId;
};

/**
 * A specialization of QMimeData for passing layers around inside
 * the application.
 */
class LayerMimeData : public QMimeData
{
Q_OBJECT
public:
	LayerMimeData(const LayerListModel *source, int id) : QMimeData(), _source(source), _id(id) {}

	const LayerListModel *source() const { return _source; }

	int layerId() const { return _id; }

	QStringList formats() const;

protected:
	QVariant retrieveData(const QString& mimeType, QVariant::Type type) const;

private:
	const LayerListModel *_source;
	int _id;
};

}

Q_DECLARE_METATYPE(net::LayerListItem)

#endif

