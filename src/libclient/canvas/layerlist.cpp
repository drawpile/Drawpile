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

#include "libclient/canvas/layerlist.h"
#include "rustpile/rustpile.h"
#include "libshared/util/qtcompat.h"

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

void LayerListModel::setLayers(const QVector<LayerListItem> &items)
{
	// See if there are any new layers we should autoselect
	int autoselect = -1;

	const uint8_t localUser = m_aclstate ? m_aclstate->localUserId() : 0;

	if(m_items.size() < items.size()) {
		for(const LayerListItem &newItem : items) {
			// O(n²) loop but the number of layers is typically small enough that
			// it doesn't matter
			bool isNew = true;
			for(const LayerListItem &oldItem : qAsConst(m_items)) {
				if(oldItem.id == newItem.id) {
					isNew = false;
					break;
				}
			}
			if(!isNew)
				continue;

			// Autoselection rules:
			// 1. If we haven't participated yet, and there is a default layer,
			//    only select the default layer
			// 2. If we haven't participated in the session yet, select any new layer
			// 3. Otherwise, select any new layer that was created by us
			// TODO implement the other rules
			if(
					newItem.creatorId() == localUser ||
					(m_autoselectAny && (
						 (m_defaultLayer>0 && newItem.id == m_defaultLayer)
						 || m_defaultLayer==0
						 )
					 )
				) {
				autoselect = newItem.id;
				break;
			}
		}
	}

	// Count root layers
	int rootLayers = 0;
	if(!items.isEmpty()) {
		++rootLayers;
		int next = items[0].right + 1;
		for(int i=1;i<items.length();++i) {
			if(items[i].left == next) {
				++rootLayers;
				next = items[i].right + 1;
			}
		}
	}

	beginResetModel();
	m_rootLayerCount = rootLayers;
	m_items = items;
	endResetModel();

	if(autoselect>=0)
		emit autoSelectRequest(autoselect);
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
}

QStringList LayerMimeData::formats() const
{
	return QStringList() << "application/x-qt-image";
}

QVariant LayerMimeData::retrieveData(const QString &mimeType, compat::RetrieveDataMetaType type) const
{
	if(compat::isImageMime(mimeType, type) && m_source->m_getlayerfn)
		return m_source->m_getlayerfn(m_id);

	return QVariant();
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
	return (censored ? rustpile::LayerAttributesMessage_FLAGS_CENSOR : 0) |
			(isolated ? rustpile::LayerAttributesMessage_FLAGS_ISOLATED : 0)
			;
}

}

