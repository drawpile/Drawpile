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

#include "layerlist.h"
#include "core/layer.h"
#include "../libshared/net/layer.h"
#include "aclfilter.h"

#include <QDebug>
#include <QStringList>
#include <QBuffer>
#include <QImage>
#include <QRegularExpression>

#include <iostream>

namespace canvas {

LayerListModel::LayerListModel(QObject *parent)
	: QAbstractListModel(parent), m_aclfilter(nullptr),
	  m_autoselectAny(true), m_defaultLayer(0), m_myId(1)
{
}
	
int LayerListModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_items.size();
}

QVariant LayerListModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && index.row() >= 0 && index.row() < m_items.size()) {
		const LayerListItem &item = m_items.at(index.row());

		switch(role) {
		case Qt::DisplayRole: return QVariant::fromValue(item);
		case TitleRole:
		case Qt::EditRole: return item.title;
		case IdRole: return item.id;
		case IsDefaultRole: return item.id == m_defaultLayer;
		case IsLockedRole: return m_aclfilter && m_aclfilter->isLayerLocked(item.id);
		case IsFixedRole: return item.fixed;
		}
	}
	return QVariant();
}

Qt::ItemFlags LayerListModel::flags(const QModelIndex& index) const
{
	if(!index.isValid())
		return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEnabled;

	return Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
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
		// note: if row is -1, the item was dropped on the parent element, which in the
		// case of the list view means the empty area below the items.
		handleMoveLayer(indexOf(ldata->layerId()), row<0 ? m_items.count() : row);
	} else {
		// TODO support new layer drops
		qWarning() << "External layer drag&drop not supported";
	}
	return false;
}

void LayerListModel::handleMoveLayer(int oldIdx, int newIdx)
{
	// Need at least two layers for this to make sense
	const int count = m_items.count();
	if(count < 2)
		return;

	// If we're moving the layer to a higher index, take into
	// account that all previous indexes shift down by one.
	int adjustedNewIdx = newIdx > oldIdx ? newIdx - 1 : newIdx;

	if(oldIdx < 0 || oldIdx >= count || adjustedNewIdx < 0 || adjustedNewIdx >= count) {
		// This can happen when a layer is deleted while someone is drag&dropping it
		qWarning("Whoops, can't move layer from %d to %d because it was just deleted!", oldIdx, newIdx);
		return;
	}

	QList<uint16_t> layers;
	layers.reserve(count);
	for(const LayerListItem &li : m_items)
		layers.append(li.id);

	layers.move(oldIdx, adjustedNewIdx);

	// Layers are shown topmost first in the list but
	// are sent bottom first in the protocol.
	for(int i=0;i<count/2;++i) {
#if QT_VERSION < QT_VERSION_CHECK(5, 13, 0)
		layers.swap(i,count-(1+i));
#else
		layers.swapItemsAt(i,count-(1+i));
#endif
	}

	emit layerCommand(protocol::MessagePtr(new protocol::LayerOrder(m_myId, layers)));
}

int LayerListModel::indexOf(uint16_t id) const
{
	for(int i=0;i<m_items.size();++i)
		if(m_items.at(i).id == id)
			return i;
	return -1;
}

QModelIndex LayerListModel::layerIndex(uint16_t id)
{
	int i = indexOf(id);
	if(i>=0)
		return index(i);
	return QModelIndex();
}

void LayerListModel::createLayer(uint16_t id, int index, const QString &title)
{
	beginInsertRows(QModelIndex(), index, index);
	m_items.insert(index, LayerListItem { id, title, 1.0, paintcore::BlendMode::MODE_NORMAL, false, false, false });
	endInsertRows();
}

void LayerListModel::deleteLayer(uint16_t id)
{
	int row = indexOf(id);
	if(row<0)
		return;

	beginRemoveRows(QModelIndex(), row, row);
	if(m_defaultLayer == id)
		m_defaultLayer = 0;
	m_items.remove(row);
	endRemoveRows();
}

void LayerListModel::clear()
{
	beginRemoveRows(QModelIndex(), 0, m_items.size());
	m_items.clear();
	m_defaultLayer = 0;
	endRemoveRows();
}

void LayerListModel::changeLayer(uint16_t id, bool censored, bool fixed, float opacity, paintcore::BlendMode::Mode blend)
{
	int row = indexOf(id);
	if(row<0)
		return;

	LayerListItem &item = m_items[row];
	item.opacity = opacity;
	item.blend = blend;
	item.censored = censored;
	item.fixed = fixed;
	const QModelIndex qmi = index(row);
	emit dataChanged(qmi, qmi);
}

void LayerListModel::retitleLayer(uint16_t id, const QString &title)
{
	int row = indexOf(id);
	if(row<0)
		return;

	LayerListItem &item = m_items[row];
	item.title = title;
	const QModelIndex qmi = index(row);
	emit dataChanged(qmi, qmi);
}

void LayerListModel::setLayerHidden(uint16_t id, bool hidden)
{
	int row = indexOf(id);
	if(row<0)
		return;

	LayerListItem &item = m_items[row];
	item.hidden = hidden;
	const QModelIndex qmi = index(row);
	emit dataChanged(qmi, qmi);
}

void LayerListModel::reorderLayers(QList<uint16_t> neworder)
{
	if(neworder.isEmpty()) {
		qWarning("reorderLayers(): empty layer list!");
		return;
	}

	QVector<LayerListItem> newitems;
	for(int j=neworder.size()-1;j>=0;--j) {
		const uint16_t id=neworder[j];
		for(int i=0;i<m_items.size();++i) {
			if(m_items[i].id == id) {
				newitems << m_items[i];
				break;
			}
		}
	}
	m_items = newitems;
	emit dataChanged(index(0), index(m_items.size()-1));
	emit layersReordered();
}

void LayerListModel::setLayers(const QVector<LayerListItem> &items)
{
	// See if there are any new layers we should autoselect
	int autoselect = -1;
	std::cout <<"Checking new layers, my id is " << int(m_myId) << std::endl;
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
					newItem.creatorId() == m_myId ||
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

	beginResetModel();
	m_items = items;
	endResetModel();

	if(autoselect>=0)
		emit autoSelectRequest(autoselect);
}

void LayerListModel::setDefaultLayer(uint16_t id)
{
	const int oldIdx = indexOf(m_defaultLayer);
	if(oldIdx >= 0) {
		emit dataChanged(index(oldIdx), index(oldIdx), QVector<int>() << IsDefaultRole);
	}

	m_defaultLayer = id;
	const int newIdx = indexOf(id);
	if(newIdx >= 0) {
		emit dataChanged(index(newIdx), index(newIdx), QVector<int>() << IsDefaultRole);
	}
}

const paintcore::Layer *LayerListModel::getLayerData(uint16_t id) const
{
	if(m_getlayerfn)
		return m_getlayerfn(id);
	return nullptr;
}

void LayerListModel::previewOpacityChange(uint16_t id, float opacity)
{
	emit layerOpacityPreview(id, opacity);
}

QStringList LayerMimeData::formats() const
{
	return QStringList() << "application/x-qt-image";
}

QVariant LayerMimeData::retrieveData(const QString &mimeType, QVariant::Type type) const
{
	Q_UNUSED(mimeType);
	if(type==QVariant::Image) {
		const paintcore::Layer *layer = m_source->getLayerData(m_id);
		if(layer)
			return layer->toCroppedImage(nullptr, nullptr);
	}

	return QVariant();
}

int LayerListModel::getAvailableLayerId() const
{
	const int prefix = m_myId << 8;
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
	return (censored ? protocol::LayerAttributes::FLAG_CENSOR : 0) |
	       (fixed ? protocol::LayerAttributes::FLAG_FIXED : 0);
}

}

