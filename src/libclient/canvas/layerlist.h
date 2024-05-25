// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_CANVAS_LAYERLIST_H
#define LIBCLIENT_CANVAS_LAYERLIST_H
extern "C" {
#include <dpmsg/blend_mode.h>
}
#include "libclient/canvas/acl.h"
#include "libclient/utils/keyframelayermodel.h"
#include "libshared/util/qtcompat.h"
#include <QAbstractItemModel>
#include <QMimeData>
#include <QSet>
#include <QVector>
#include <functional>

namespace drawdance {
class LayerPropsList;
}

namespace canvas {

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
	DP_BlendMode blend;

	//! Layer hidden flag (local only)
	bool hidden;

	//! Layer contents are censored
	bool censored;

	//! Layer is actually censored, but was revealed anyway
	bool revealed;

	//! Isolated (not pass-through) group?
	bool isolated;

	//! Is this a layer group?
	bool group;

	//! Number of child layers
	uint16_t children;

	//! Index in parent group
	uint16_t relIndex;

	//! Left index (MPTT)
	int left;

	//! Right index (MPTT)
	int right;

	//! Get the LayerAttributes flags as a bitfield
	uint8_t attributeFlags() const;

	//! Get the ID of the user who created this layer
	uint8_t creatorId() const { return uint8_t((id & 0xff00) >> 8); }

	bool actuallyCensored() const { return censored || revealed; }
};

}

Q_DECLARE_TYPEINFO(canvas::LayerListItem, Q_MOVABLE_TYPE);

namespace canvas {

typedef std::function<QImage(int id)> GetLayerFunction;

class LayerListModel final : public QAbstractItemModel {
	Q_OBJECT
	friend class LayerMimeData;

public:
	enum LayerListRoles {
		IdRole = Qt::UserRole + 1,
		TitleRole,
		IsDefaultRole,
		IsLockedRole,
		IsGroupRole,
		IsEmptyRole,
		IsHiddenInFrameRole,
		IsHiddenInTreeRole,
		IsCensoredInTreeRole,
	};

	LayerListModel(QObject *parent = nullptr);

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	Qt::DropActions supportedDropActions() const override;
	QStringList mimeTypes() const override;
	QMimeData *mimeData(const QModelIndexList &indexes) const override;
	bool dropMimeData(
		const QMimeData *data, Qt::DropAction action, int row, int column,
		const QModelIndex &parent) override;
	QModelIndex index(
		int row, int column,
		const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex &index) const override;

	QModelIndex layerIndex(uint16_t id) const;
	const QVector<LayerListItem> &layerItems() const { return m_items; }

	void setLayerGetter(GetLayerFunction fn) { m_getlayerfn = fn; }
	void setAclState(AclState *state) { m_aclstate = state; }

	/**
	 * Enable/disable any (not just own) layer autoselect requests
	 *
	 * When the local user hasn't yet drawn anything, any newly created layer
	 * should be selected.
	 */
	void setAutoselectAny(bool autoselect) { m_autoselectAny = autoselect; }

	void setLayerIdToSelect(int layerId) { m_layerIdToSelect = layerId; }

	/**
	 * @brief Get the default layer to select when logging in
	 * Zero means no default.
	 */
	uint16_t defaultLayer() const { return m_defaultLayer; }
	void setDefaultLayer(uint16_t id);

	AclState::Layer layerAcl(uint16_t id);

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

	/**
	 * Find the nearest layer to the one given.
	 *
	 * This is used to find a layer to auto-select when the
	 * current selection is deleted.
	 *
	 * @returns layer ID or 0 if there are no layers
	 */
	int findNearestLayer(int layerId) const;

	bool isLayerVisibleInFrame(int layerId) const
	{
		return m_frameLayers.contains(layerId);
	}

	KeyFrameLayerModel *toKeyFrameLayerModel(
		int rootLayerId, const QHash<int, bool> &layerVisibility) const;

public slots:
	void setLayers(
		const drawdance::LayerPropsList &lpl, const QSet<int> &revealedLayers);
	void setLayersVisibleInFrame(const QSet<int> &layers, bool frameMode);

signals:
	void layersChanged(const QVector<LayerListItem> &items);

	//! A new layer was created that should be automatically selected
	void autoSelectRequest(int);

	//! Emitted when layers are manually reordered
	void moveRequested(int sourceId, int targetId, bool intoGroup, bool below);

	void layersVisibleInFrameChanged();

private:
	static int searchAvailableLayerId(const QSet<int> &takenIds, int contextId);

	void flattenKeyFrameLayer(
		QVector<KeyFrameLayerItem> &items, int &index, int &layerIndex,
		int relIndex, const QHash<int, bool> &layerVisibiltiy) const;

	QVector<LayerListItem> m_items;
	QSet<int> m_frameLayers;
	GetLayerFunction m_getlayerfn;
	AclState *m_aclstate;
	int m_rootLayerCount;
	uint16_t m_defaultLayer;
	bool m_autoselectAny;
	bool m_frameMode;
	int m_layerIdToSelect;
};

/**
 * A specialization of QMimeData for passing layers around inside
 * the application.
 */
class LayerMimeData final : public QMimeData {
	Q_OBJECT
public:
	LayerMimeData(const LayerListModel *source, uint16_t id)
		: QMimeData()
		, m_source(source)
		, m_id(id)
	{
	}

	const LayerListModel *source() const { return m_source; }

	uint16_t layerId() const { return m_id; }

	QStringList formats() const override;

protected:
	QVariant retrieveData(
		const QString &mimeType,
		compat::RetrieveDataMetaType type) const override;

private:
	const LayerListModel *m_source;
	uint16_t m_id;

	QVariant retrieveImageData(bool isImageType) const;
};

}

Q_DECLARE_METATYPE(canvas::LayerListItem)

#endif
