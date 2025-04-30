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
#include <QColor>
#include <QHash>
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
	int id;

	//! Layer title
	QString title;

	//! Layer color marker
	QColor color;

	//! Layer opacity
	float opacity;

	//! Blending mode
	DP_BlendMode blend;

	//! Sketch mode opacity
	float sketchOpacity;

	//! Sketch mode tint
	QColor sketchTint;

	//! Layer hidden flag (local only)
	bool hidden;

	//! Layer contents are censored
	bool censored;

	//! Layer is actually censored, but was revealed anyway
	bool revealed;

	//! Isolated (not pass-through) group?
	bool isolated;

	//! Does this layer clip to the one below?
	bool clip;

	//! Is this a layer group?
	bool group;

	//! Number of child layers
	int children;

	//! Index in parent group
	int relIndex;

	//! Left index (MPTT)
	int left;

	//! Right index (MPTT)
	int right;

	//! Get a null layer list item with everything set 0 or the default.
	static LayerListItem null();

	//! Get the LayerAttributes flags as a bitfield
	uint8_t attributeFlags() const;

	//! Get the ID of the user who created this layer
	uint8_t creatorId() const;

	//! Get the layer element ID, without creator ID
	int elementId() const;

	bool actuallyCensored() const { return censored || revealed; }

	QString titleWithColor() const { return makeTitleWithColor(title, color); }

	static QString
	makeTitleWithColor(const QString &title, const QColor &color);
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
		IsFillSourceRole,
		CheckModeRole,
		CheckStateRole,
		IsSketchModeRole,
		OwnerIdRole,
		ColorRole,
		IsClipRole,
		IsAtBottomRole,
	};

	enum CheckState {
		Unchecked = Qt::Unchecked,
		PartiallyChecked = Qt::PartiallyChecked,
		Checked = Qt::Checked,
		NotApplicable,
		NotCheckable,
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

	QModelIndex layerIndex(int id) const;
	const QVector<LayerListItem> &layerItems() const { return m_items; }

	void setLayerGetter(GetLayerFunction fn) { m_getlayerfn = fn; }
	void setAclState(AclState *aclstate);

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
	int defaultLayer() const { return m_defaultLayer; }
	void setDefaultLayer(int id);

	int fillSourceLayerId() const { return m_fillSourceLayerId; }
	int layerIdLimit() const { return m_layerIdLimit; }

	AclState::Layer layerAcl(int id);

	/**
	 * @brief Find a free layer ID
	 * @return layer ID or 0 if all are taken
	 */
	int getAvailableLayerId() const;
	QVector<int> getAvailableLayerIds(int count) const;

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

	QModelIndex findHighestLayer(const QSet<int> &layerIds) const;

	QSet<int> topLevelSelectedIds(const QSet<int> &layerIds) const;

	static bool
	isTopLevelSelection(const QSet<int> &layerIds, const QModelIndex &idx);

	static QModelIndex
	toTopLevelSelection(const QSet<int> &layerIds, const QModelIndex &idx);

	bool isLayerVisibleInFrame(int layerId) const
	{
		return m_frameLayers.contains(layerId);
	}

	KeyFrameLayerModel *toKeyFrameLayerModel(
		int rootLayerId, const QHash<int, bool> &layerVisibility) const;

	// Retrieves all modifiable layers under the given id. That is, it will
	// return the ids of non-group, non-locked, non-hidden, non-censored layers.
	QSet<int> getModifiableLayers(const QSet<int> &layerIds) const;

	bool isCheckMode() const { return m_checkMode; }
	void initCheckedLayers(const QSet<int> &initialLayerIds);
	void clearCheckedLayers();
	void setAllChecked(bool checked);

	// Retrieves all checked layers, not including groups.
	QSet<int> checkedLayers() const;

	bool isLayerCheckStateToggleable(const QModelIndex &idx) const;

public slots:
	void setLayers(
		const drawdance::LayerPropsList &lpl, const QSet<int> &revealedLayers);
	void setLayersVisibleInFrame(const QSet<int> &layers, int viewMode);
	void setLayerChecked(int layerId, bool checked);
	void setFillSourceLayerId(int fillSourceLayerId);
	void updateFeatureLimit(DP_FeatureLimit featureLimit, int value);

signals:
	void layersChanged(const QVector<LayerListItem> &items);

	//! A new layer was created that should be automatically selected
	void autoSelectRequest(int);

	//! Emitted when layers are manually reordered
	void moveRequested(
		const QVector<int> &sourceIds, int targetId, bool intoGroup,
		bool below);

	void layersVisibleInFrameChanged();
	void layerCheckStateToggled();
	void fillSourceSet(int layerId);

private slots:
	void updateCheckedLayerAcl(int layerId);

private:
	int searchAvailableLayerId(
		const QSet<int> &takenIds, const QVector<int> &takenPerUser,
		unsigned int contextId) const;

	CheckState flattenLayerList(
		QVector<LayerListItem> &newItems, QHash<int, CheckState> &checkStates,
		int &index, const drawdance::LayerPropsList &lpl,
		const QSet<int> &revealedLayers, bool parentCensored);

	void flattenKeyFrameLayer(
		QVector<KeyFrameLayerItem> &items, int &index, int &layerIndex,
		int relIndex, const QHash<int, bool> &layerVisibiltiy) const;

	void gatherCheckedLayers(
		QHash<int, CheckState> &checkStates, const QModelIndex &idx);

	CheckState fillCheckStates(
		QHash<int, CheckState> &checkStates, const QModelIndex &idx);

	CheckState getGroupCheckState(
		QHash<int, CheckState> &checkStates, const QModelIndex &idx);

	CheckState setLayerCheckedChildren(const QModelIndex &idx, bool checked);
	void refreshLayerCheckedParents(const QModelIndex &idx);
	CheckState updateLayerChecked(const QModelIndex &idx, bool checked);
	void updateLayerCheckState(const QModelIndex &idx, CheckState checkState);
	static bool isLayerCheckable(const QModelIndex &idx);

	void
	gatherModifiableLayers(QSet<int> &layerIds, const QModelIndex &idx) const;

	bool isLayerOrClippingGroupHidden(
		const QModelIndex &index, const LayerListItem &item) const;

	QVector<LayerListItem> m_items;
	QSet<int> m_frameLayers;
	QHash<int, CheckState> m_checkStates;
	GetLayerFunction m_getlayerfn = nullptr;
	AclState *m_aclstate = nullptr;
	int m_rootLayerCount = 0;
	int m_defaultLayer = 0;
	bool m_autoselectAny = true;
	bool m_checkMode = false;
	int m_viewMode;
	int m_layerIdToSelect = 0;
	int m_fillSourceLayerId = 0;
	int m_layerIdLimit;
};

/**
 * A specialization of QMimeData for passing layers around inside
 * the application.
 */
class LayerMimeData final : public QMimeData {
	Q_OBJECT
public:
	LayerMimeData(const LayerListModel *source)
		: QMimeData()
		, m_source(source)
	{
	}

	const LayerListModel *source() const { return m_source; }

	void insertLayerId(int layerId) { m_layerIds.insert(layerId); }
	const QSet<int> &layerIds() const { return m_layerIds; }

	QStringList formats() const override;

protected:
	QVariant retrieveData(
		const QString &mimeType,
		compat::RetrieveDataMetaType type) const override;

private:
	const LayerListModel *m_source;
	QSet<int> m_layerIds;

	QVariant retrieveImageData(bool isImageType) const;
};

}

Q_DECLARE_METATYPE(canvas::LayerListItem)

#endif
