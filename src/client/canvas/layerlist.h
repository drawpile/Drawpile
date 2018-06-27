/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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

#include "core/blendmodes.h"
#include "features.h"

#include <QAbstractListModel>
#include <QMimeData>
#include <QVector>

#include <functional>

namespace protocol {
	class MessagePtr;

}
namespace paintcore {
	class Layer;
}

namespace canvas {

class AclFilter;

struct LayerListItem {
	//! Layer ID
	int id;
	
	//! Layer title
	QString title;
	
	//! Layer opacity
	float opacity;
	
	//! Blending mode
	paintcore::BlendMode::Mode blend;

	//! Layer hidden flag (local only)
	bool hidden;

	//! Layer is flagged for censoring
	bool censored;
};

}

Q_DECLARE_TYPEINFO(canvas::LayerListItem, Q_MOVABLE_TYPE);

namespace canvas {

typedef std::function<const paintcore::Layer*(int id)> GetLayerFunction;

class LayerListModel : public QAbstractListModel {
	Q_OBJECT
public:
	enum LayerListRoles {
		IdRole = Qt::UserRole + 1,
		TitleRole,
		IsDefaultRole,
		IsLockedRole
	};

	LayerListModel(QObject *parent=nullptr);
	
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
	void changeLayer(int id, bool censored, float opacity, paintcore::BlendMode::Mode blend);
	void retitleLayer(int id, const QString &title);
	void setLayerHidden(int id, bool hidden);
	void reorderLayers(QList<uint16_t> neworder);
	
	QVector<LayerListItem> getLayers() const { return m_items; }
	void setLayers(const QVector<LayerListItem> &items);

	void previewOpacityChange(int id, float opacity);

	void setLayerGetter(GetLayerFunction fn) { m_getlayerfn = fn; }
	void setAclFilter(AclFilter *filter) { m_aclfilter = filter; }
	const paintcore::Layer *getLayerData(int id) const;

	int myId() const { return m_myId; }
	void setMyId(int id) { m_myId = id; }

	/**
	 * @brief Get the default layer to select when logging in
	 * Zero means no default.
	 */
	int defaultLayer() const { return m_defaultLayer; }
	void setDefaultLayer(int id);

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
	void layersReordered();

	//! Emitted when layers are manually reordered
	void layerCommand(protocol::MessagePtr msg);

	//! Request local change of layer opacity for preview purpose
	void layerOpacityPreview(int id, float opacity);

private:
	void handleMoveLayer(int idx, int afterIdx);

	int indexOf(int id) const;

	QVector<LayerListItem> m_items;
	GetLayerFunction m_getlayerfn;
	AclFilter *m_aclfilter;
	int m_defaultLayer;
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

Q_DECLARE_METATYPE(canvas::LayerListItem)

#endif

