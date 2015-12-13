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

#include "layerlist.h"
#include "core/layer.h"
#include "../shared/net/layer.h"

#include <QDebug>
#include <QStringList>
#include <QBuffer>
#include <QImage>
#include <QRegularExpression>

namespace canvas {

using paintcore::LayerInfo;

LayerListModel::LayerListModel(QObject *parent)
	: QAbstractListModel(parent)
{
}
	
int LayerListModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_layers.size();
}

QVariant LayerListModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && index.row() >= 0 && index.row() < m_layers.size()) {
		const LayerInfo &item = m_layers.at(index.row());

		switch(role) {
		case Qt::DisplayRole: return QVariant::fromValue(item);
		case TitleRole:
		case Qt::EditRole: return item.title;
		case IdRole: return item.id;
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
	return new LayerMimeData(this, indexes[0].data().value<LayerInfo>().id);
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
		handleMoveLayer(indexOf(ldata->layerId()), row<0 ? m_layers.size() : row);
	} else {
		// TODO support new layer drops
		qWarning() << "External layer drag&drop not supported";
	}
	return false;
}

void LayerListModel::handleMoveLayer(int oldIdx, int newIdx)
{
	// Need at least two layers for this to make sense
	const int count = m_layers.size();
	if(count < 2)
		return;

	QList<uint16_t> layers;
	layers.reserve(count);
	for(const LayerInfo &li : m_layers)
		layers.append(li.id);

	if(newIdx>oldIdx)
		--newIdx;

	layers.move(oldIdx, newIdx);

	// Layers are shown topmost first in the list but
	// are sent bottom first in the protocol.
	for(int i=0;i<count/2;++i)
		layers.swap(i,count-(1+i));

	emit layerCommand(protocol::MessagePtr(new protocol::LayerOrder(0, layers)));
}

int LayerListModel::indexOf(int id) const
{
	for(int i=0;i<m_layers.size();++i)
		if(m_layers.at(i).id == id)
			return i;
	return -1;
}

QModelIndex LayerListModel::layerIndex(int id)
{
	int i = indexOf(id);
	if(i>=0)
		return index(i);
	return QModelIndex();
}

void LayerListModel::addLayer(int idx, const paintcore::LayerInfo &layer)
{
	// Reverse index since the topmost layer is shown first
	idx = m_layers.size() - idx;

	beginInsertRows(QModelIndex(), idx, idx);
	m_layers.insert(idx, layer);
	endInsertRows();
}

void LayerListModel::deleteLayer(int idx)
{
	// Reverse index
	idx = m_layers.size() - 1 - idx;

	Q_ASSERT(idx>=0 && idx<m_layers.size());
	beginRemoveRows(QModelIndex(), idx, idx);
	m_layers.removeAt(idx);
	endRemoveRows();

}

void LayerListModel::updateLayer(int idx, const paintcore::LayerInfo &layer)
{
	// Reverse index
	idx = m_layers.size() - 1 - idx;
	m_layers[idx] = layer;

	const QModelIndex qmi = index(idx);
	emit dataChanged(qmi, qmi);
}

void LayerListModel::updateLayers(const QList<paintcore::LayerInfo> &layers)
{
	beginResetModel();
	m_layers = layers;
	endResetModel();
}

const paintcore::Layer *LayerListModel::getLayerData(int id) const
{
	if(m_getlayerfn)
		return m_getlayerfn(id);
	return nullptr;
}

void LayerListModel::previewOpacityChange(int id, float opacity)
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
		const paintcore::Layer *layer = _source->getLayerData(_id);
		if(layer)
			return layer->toCroppedImage(0, 0);
	}

	return QVariant();
}

int LayerListModel::getAvailableLayerId() const
{
	const int prefix = m_myId << 8;
	QList<int> takenIds;
	for(const LayerInfo &item : m_layers) {
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
	for(const LayerInfo &l : m_layers) {
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

}
