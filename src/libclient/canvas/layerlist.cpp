/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2021 Calle Laakkonen

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

extern "C" {
#include <dpmsg/message.h>
#include <dpengine/pixels.h>
}

#include "libclient/canvas/layerlist.h"
#include "libclient/drawdance/layerpropslist.h"

#include <QDebug>
#include <QImage>
#include <QStringList>
#include <QRegularExpression>

namespace canvas {

LayerListModel::LayerListModel(QObject *parent)
	: QAbstractItemModel(parent), m_aclstate(nullptr),
	  m_rootLayerCount(0), m_defaultLayer(0), m_autoselectAny(true), m_frameMode(false)
{
}

QVariant LayerListModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid())
		return QVariant();

	const LayerListItem &item = m_items.at(index.internalId());

	switch(role) {
	case Qt::DisplayRole: return QVariant::fromValue(item);
	case TitleRole:
	case Qt::EditRole: return item.title;
	case IdRole: return item.id;
	case IsDefaultRole: return item.id == m_defaultLayer;
	case IsLockedRole: return (m_frameMode && !m_frameLayers.contains(item.frameId)) || (m_aclstate && m_aclstate->isLayerLocked(item.id));
	case IsGroupRole: return item.group;
	}

	return QVariant();
}

Qt::ItemFlags LayerListModel::flags(const QModelIndex& index) const
{
	if(index.isValid()) {
		const bool isGroup = m_items.at(index.internalId()).group;

		return Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | (isGroup ? Qt::ItemIsDropEnabled : Qt::NoItemFlags);
	}
	return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEnabled;
}

Qt::DropActions LayerListModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

QStringList LayerListModel::mimeTypes() const {
		return QStringList() << "application/x-qt-image";
}

QMimeData *LayerListModel::mimeData(const QModelIndexList& indexes) const
{
	return new LayerMimeData(this, indexes[0].data().value<LayerListItem>().id);
}

bool LayerListModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
	Q_UNUSED(action);
	Q_UNUSED(column);
	Q_UNUSED(parent);

	const LayerMimeData *ldata = qobject_cast<const LayerMimeData*>(data);
	if(ldata && ldata->source() == this) {
		if(m_items.size() < 2)
			return false;

		int targetId;
		bool intoGroup = false;
		bool below = false;
		if(row < 0) {
			if(parent.isValid()) {
				// row<0, valid parent: move into the group
				targetId = m_items.at(parent.internalId()).id;
				intoGroup = true;
			} else {
				// row<0, no parent: the empty area below the layer list (move to root/bottom)
				targetId = m_items.at(index(rowCount()-1, 0).internalId()).id;
				below = true;
			}
		} else {
			const int children = rowCount(parent);
			if(row >= children) {
				// row >= number of children in group (or root): move below the last item in the group
				targetId = m_items.at(index(children-1, 0, parent).internalId()).id;
				below = true;
			} else {
				// the standard case: move above this layer
				targetId = m_items.at(index(row, 0, parent).internalId()).id;
			}
		}

#if 0
		qInfo("Drop row=%d (parent row=%d, id=%d, rowCount=%d)", row, parent.row(), parent.internalId(), rowCount(parent));
		for(int i=0;i<m_items.size();++i) {
			qInfo("[%d] id=%d, children=%d", i, m_items.at(i).id, m_items.at(i).children);
		}
		qInfo("Requesting move of %d to %s %d, into=%d", ldata->layerId(), below ? "below" : "above", targetId, intoGroup);
#endif

		emit moveRequested(ldata->layerId(), targetId, intoGroup, below);

	} else {
		// TODO support new layer drops
		qWarning("External layer drag&drop not supported");
	}
	return false;
}

QModelIndex LayerListModel::layerIndex(uint16_t id) const
{
	for(int i=0;i<m_items.size();++i) {
		if(m_items.at(i).id == id) {
			return createIndex(m_items.at(i).relIndex, 0, i);
		}
	}

	return QModelIndex();
}

int LayerListModel::findNearestLayer(int layerId) const
{
	const auto i = layerIndex(layerId);
	const int row = i.row();

	auto nearest = i.sibling(row+1, 0);
	if(nearest.isValid())
		return m_items.at(nearest.internalId()).id;

	nearest = i.sibling(row-1, 0);
	if(nearest.isValid())
		return m_items.at(nearest.internalId()).id;

	nearest = i.parent();
	if(nearest.isValid())
		return m_items.at(nearest.internalId()).id;

	return 0;
}

int LayerListModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid()) {
		return m_items.at(parent.internalId()).children;
	}

	return m_rootLayerCount;
}

int LayerListModel::columnCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent);
	return 1;
}

QModelIndex LayerListModel::parent(const QModelIndex &index) const
{
	if(!index.isValid())
		return QModelIndex();

	int seek = index.internalId();

	const int right = m_items.at(seek).right;
	while(--seek >= 0) {
		if(m_items.at(seek).right > right) {
			return createIndex(m_items.at(seek).relIndex, 0, seek);
		}
	}

	return QModelIndex();
}

QModelIndex LayerListModel::index(int row, int column, const QModelIndex &parent) const
{
	if(m_items.isEmpty() || row < 0 || column != 0)
		return QModelIndex();

	int cursor;

	if(parent.isValid()) {
		Q_ASSERT(m_items.at(parent.internalId()).group);
		cursor = parent.internalId();
		if(row >= m_items.at(cursor).children)
			return QModelIndex();

		cursor += 1; // point to the first child element

	} else {
		if(row >= m_rootLayerCount)
			return QModelIndex();

		cursor = 0;
	}

	int next = m_items.at(cursor).right + 1;

	int i = 0;
	while(i < row) {
		while(cursor < m_items.size() && m_items.at(cursor).left < next)
			++cursor;

		if(cursor == m_items.size() || m_items.at(cursor).left > next)
			return QModelIndex();

		next = m_items.at(cursor).right + 1;
		++i;
	}

#if 0
	qInfo("index(row=%d), parent row=%d (id=%d, children=%d, group=%d), relIndex=%d, cursor=%d, left=%d, right=%d",
		  row,
		  parent.row(), int(parent.internalId()), m_items.at(parent.internalId()).children, m_items.at(parent.internalId()).group,
		  m_items.at(cursor).relIndex, cursor,
		  m_items.at(cursor).left, m_items.at(cursor).right
		  );
#endif

	Q_ASSERT(m_items.at(cursor).relIndex == row);

	return createIndex(row, column, cursor);
}

static LayerListItem makeItem(
	const drawdance::LayerProps &lp, int frameId, bool isGroup,
	const drawdance::LayerPropsList &children, int relIndex, int left,
	int right)
{
	return LayerListItem{
		uint16_t(lp.id()), uint16_t(frameId), lp.title(),
		float(lp.opacity()) / float(DP_BIT15), DP_BlendMode(lp.blendMode()),
		lp.hidden(), lp.censored(), lp.isolated(), isGroup,
		uint16_t(isGroup ? children.count() : 0), uint16_t(relIndex),
		left, right};
}

static void flattenLayerList(
	QVector<LayerListItem> &newItems, int &index,
	const drawdance::LayerPropsList lpl, int frameId)
{
	int count = lpl.count();
	for(int i = 0; i < count; ++i) {
		// Layers are shown in reverse in the UI.
		drawdance::LayerProps lp = lpl.at(count - i - 1);
		drawdance::LayerPropsList children;
		if(lp.isGroup(&children)) {
			int nextFrameId = frameId == 0 && lp.isolated() ? lp.id() : frameId;
			int pos = newItems.count();
			newItems.append(makeItem(
				lp, nextFrameId, true, children, i, index, -1));
			++index;
			flattenLayerList(newItems, index, children, nextFrameId);
			newItems[pos].right = index;
			++index;
		} else {
			newItems.append(makeItem(
				lp, frameId, false, children, i, index, index + 1));
			index += 2;
		}
	}
}

static bool isNewLayerId(
	const QVector<LayerListItem> &oldItems, const LayerListItem &newItem)
{
	// O(n²) loop but the number of layers is typically small enough that
	// it doesn't matter
	int id = newItem.id;
	for(const LayerListItem &item : oldItems) {
		if(item.id == id) {
			return false;
		}
	}
	return true;
}

static int getAutoselect(
	uint8_t localUser, bool autoselectAny, int defaultLayer,
	const QVector<LayerListItem> &oldItems, const QVector<LayerListItem> &newItems)
{
	if(autoselectAny) {
		// We haven't participated yet: select the default layer if it exists.
		if(defaultLayer > 0) {
			for(const LayerListItem &newItem : newItems) {
				if(newItem.id == defaultLayer) {
					return isNewLayerId(oldItems, newItem);
				}
			}
		}
		// No default layer, just pick latest newly created one we can find.
		for(const LayerListItem &newItem : newItems) {
			if(isNewLayerId(oldItems, newItem)) {
				return newItem.id;
			}
		}
	} else {
		// We already participated: we might have just created a new layer,
		// try to select that one.
		for(const LayerListItem &newItem : newItems) {
			if(isNewLayerId(oldItems, newItem) && newItem.creatorId() == localUser) {
				return newItem.id;
			}
		}
	}
	// Don't select a different layer.
	return -1;
}

void LayerListModel::setLayers(const drawdance::LayerPropsList &lpl)
{
	QVector<LayerListItem> newItems;
	int index = 0;
	flattenLayerList(newItems, index, lpl, 0);

	const uint8_t localUser = m_aclstate ? m_aclstate->localUserId() : 0;
	int autoselect = getAutoselect(
		localUser, m_autoselectAny, m_defaultLayer, m_items, newItems);

	beginResetModel();
	m_rootLayerCount = lpl.count();
	m_items = newItems;
	endResetModel();

	if(autoselect >= 0) {
		emit autoSelectRequest(autoselect);
	}
}

void LayerListModel::setLayersVisibleInFrame(const QVector<int> &layers, bool frameMode)
{
	beginResetModel(); //FIXME don't reset the whole model
	m_frameLayers = layers;
	m_frameMode = frameMode;
	endResetModel();
}

void LayerListModel::setDefaultLayer(uint16_t id)
{
	if(id == m_defaultLayer)
		return;

	const QVector<int> role { IsDefaultRole };

	const auto oldIdx = layerIndex(m_defaultLayer);
	if(oldIdx.isValid())
		emit dataChanged(oldIdx, oldIdx, role);

	m_defaultLayer = id;

	const auto newIdx = layerIndex(m_defaultLayer);
	if(newIdx.isValid())
		emit dataChanged(newIdx, newIdx, role);

	if(m_defaultLayer > 0 && m_autoselectAny) {
		emit autoSelectRequest(m_defaultLayer);
	}
}

QStringList LayerMimeData::formats() const
{
	return QStringList() << "application/x-qt-image";
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QVariant LayerMimeData::retrieveData(const QString &mimeType, QVariant::Type type) const
{
	Q_UNUSED(mimeType);
	return retrieveImageData(type == QVariant::Image);
}
#else
QVariant LayerMimeData::retrieveData(const QString &mimeType, QMetaType type) const
{
	Q_UNUSED(mimeType);
	return retrieveImageData(type.id() == QMetaType::QImage);
}
#endif

QVariant LayerMimeData::retrieveImageData(bool isImageType) const
{
	if(isImageType && m_source->m_getlayerfn) {
		return m_source->m_getlayerfn(m_id);
	}
	return QVariant{};
}

int LayerListModel::getAvailableLayerId() const
{
	Q_ASSERT(m_aclstate);

	const int prefix = int(m_aclstate->localUserId()) << 8;
	QList<int> takenIds;
	for(const LayerListItem &item : m_items) {
		if((item.id & 0xff00) == prefix)
			takenIds.append(item.id);
	}

	for(int i=0;i<256;++i) {
		int id = prefix | i;
		if(!takenIds.contains(id))
			return id;
	}

	return 0;
}

QString LayerListModel::getAvailableLayerName(QString basename) const
{
	// Return a layer name of format "basename n" where n is one bigger than the
	// biggest suffix number of layers named "basename n".

	// First, strip suffix number from the basename (if it exists)

	QRegularExpression suffixNumRe("(\\d+)$");
	{
		auto m = suffixNumRe.match(basename);
		if(m.hasMatch()) {
			basename = basename.mid(0, m.capturedStart()).trimmed();
		}
	}

	// Find the biggest suffix in the layer stack
	int suffix = 0;
	for(const LayerListItem &l : m_items) {
		auto m = suffixNumRe.match(l.title);
		if(m.hasMatch()) {
			if(l.title.startsWith(basename)) {
				suffix = qMax(suffix, m.captured(1).toInt());
			}
		}
	}

	// Make unique name
	return QString("%2 %1").arg(suffix+1).arg(basename);
}

uint8_t LayerListItem::attributeFlags() const
{
	return (censored ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_CENSOR : 0) |
			(isolated ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_ISOLATED : 0)
			;
}

}

