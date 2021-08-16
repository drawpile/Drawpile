/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2019 Calle Laakkonen

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

#include "../core/blendmodes.h"
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
	// Note: normally, layer ID range is from 0 to 0xffff, but internal
	// layers use values outside that range. However, internal layers are not
	// shown in the layer list.
	uint16_t id;
	
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

	//! This is a fixed background/foreground layer
	bool fixed;

	//! Get the LayerAttributes flags as a bitfield
	uint8_t attributeFlags() const;

	//! Get the ID of the user who created this layer
	uint8_t creatorId() const { return uint8_t((id & 0xff00) >> 8); }
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
		IsLockedRole,
		IsFixedRole
	};

	LayerListModel(QObject *parent=nullptr);
	
	int rowCount(const QModelIndex &parent=QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const;
	Qt::ItemFlags flags(const QModelIndex& index) const;
	Qt::DropActions supportedDropActions() const;
	QStringList mimeTypes() const;
	QMimeData *mimeData(const QModelIndexList& indexes) const;
	bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);

	QModelIndex layerIndex(uint16_t id);
	
	void clear();
	void createLayer(uint16_t id, int index, const QString &title);
	void deleteLayer(uint16_t id);
	void changeLayer(uint16_t id, bool censored, bool fixed, float opacity, paintcore::BlendMode::Mode blend);
	void retitleLayer(uint16_t id, const QString &title);
	void setLayerHidden(uint16_t id, bool hidden);
	void reorderLayers(QList<uint16_t> neworder);
	
	QVector<LayerListItem> getLayers() const { return m_items; }

	void previewOpacityChange(uint16_t id, float opacity);

	void setLayerGetter(GetLayerFunction fn) { m_getlayerfn = fn; }
	void setAclFilter(AclFilter *filter) { m_aclfilter = filter; }
	const paintcore::Layer *getLayerData(uint16_t id) const;

	uint8_t myId() const { return m_myId; }
	void setMyId(uint8_t id) { m_myId = id; }

	/**
	 * Enable/disable any (not just own) layer autoselect requests
	 *
	 * When the local user hasn't yet drawn anything, any newly created layer
	 * should be selected.
	 */
	void setAutoselectAny(bool autoselect) { m_autoselectAny = autoselect; }

	/**
	 * @brief Get the default layer to select when logging in
	 * Zero means no default.
	 */
	uint16_t defaultLayer() const { return m_defaultLayer; }
	void setDefaultLayer(uint16_t id);

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
	void setLayers(const QVector<LayerListItem> &items);

signals:
	void layersReordered();

	//! A new layer was created that should be automatically selected
	void autoSelectRequest(int);

	//! Emitted when layers are manually reordered
	void layerCommand(protocol::MessagePtr msg);

	//! Request local change of layer opacity for preview purpose
	void layerOpacityPreview(int id, float opacity);

private:
	void handleMoveLayer(int idx, int afterIdx);

	int indexOf(uint16_t id) const;

	QVector<LayerListItem> m_items;
	GetLayerFunction m_getlayerfn;
	AclFilter *m_aclfilter;
	bool m_autoselectAny;
	uint16_t m_defaultLayer;
	uint8_t m_myId;
};

/**
 * A specialization of QMimeData for passing layers around inside
 * the application.
 */
class LayerMimeData : public QMimeData
{
Q_OBJECT
public:
	LayerMimeData(const LayerListModel *source, uint16_t id)
		: QMimeData(), m_source(source), m_id(id) {}

	const LayerListModel *source() const { return m_source; }

	uint16_t layerId() const { return m_id; }

	QStringList formats() const;

protected:
	QVariant retrieveData(const QString& mimeType, QVariant::Type type) const;

private:
	const LayerListModel *m_source;
	uint16_t m_id;
};

}

Q_DECLARE_METATYPE(canvas::LayerListItem)

#endif

