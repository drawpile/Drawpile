/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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

#include "core/layer.h"
#include "core/blendmodes.h"

#include <QAbstractListModel>
#include <QMimeData>
#include <QVector>

#include <functional>

namespace protocol {
	class MessagePtr;
}

namespace canvas {

typedef std::function<const paintcore::Layer*(int id)> GetLayerFunction;

class LayerListModel : public QAbstractListModel {
	Q_OBJECT
public:
	enum LayerListRoles {
		IdRole = Qt::UserRole + 1,
		TitleRole,
	};

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
	
	void previewOpacityChange(int id, float opacity);

	void setLayerGetter(GetLayerFunction fn) { m_getlayerfn = fn; }
	const paintcore::Layer *getLayerData(int id) const;

	int myId() const { return m_myId; }
	void setMyId(int id) { m_myId = id; }

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

public slots:
	void addLayer(int idx, const paintcore::LayerInfo &layer);
	void deleteLayer(int idx);
	void updateLayer(int idx, const paintcore::LayerInfo &layer);
	void updateLayers(const QList<paintcore::LayerInfo> &layers);

signals:
	void layersReordered();

	//! Emitted when layers are manually reordered
	void layerCommand(protocol::MessagePtr msg);

	//! Request local change of layer opacity for preview purpose
	void layerOpacityPreview(int id, float opacity);

private:
	void handleMoveLayer(int idx, int afterIdx);

	int indexOf(int id) const;
	
	QList<paintcore::LayerInfo> m_layers;
	GetLayerFunction m_getlayerfn;
	int m_myId;
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

#endif

