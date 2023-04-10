// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/keyframelayermodel.h"


KeyFrameLayerModel::KeyFrameLayerModel(
	const QVector<KeyFrameLayerItem> &items, QObject *parent)
	: QAbstractItemModel{parent}
	, m_items{items}
{
}

QVariant KeyFrameLayerModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid()) {
		const KeyFrameLayerItem &item = m_items.at(index.internalId());
		switch(role) {
		case Qt::DisplayRole:
			return QVariant::fromValue(item);
		case Qt::ToolTipRole:
			switch(item.visibility) {
			case KeyFrameLayerItem::Visibility::Hidden:
				return tr("Visibility: Hidden");
			case KeyFrameLayerItem::Visibility::Revealed:
				return tr("Visibility: Revealed");
			default:
				return tr("Visibility: Parent");
			}
		case IsVisibleRole:
			switch(item.visibility) {
			case KeyFrameLayerItem::Visibility::Hidden:
				return false;
			case KeyFrameLayerItem::Visibility::Revealed:
				return true;
			default: {
				QModelIndex parent = index.parent();
				return !parent.isValid() || parent.data(IsVisibleRole).toBool();
			}
			}
		}
	}
	return QVariant{};
}

Qt::ItemFlags KeyFrameLayerModel::flags(const QModelIndex &index) const
{
	return QAbstractItemModel::flags(index) & ~Qt::ItemIsSelectable;
}

int KeyFrameLayerModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid()) {
		return m_items.at(parent.internalId()).children;
	} else {
		return 1;
	}
}

int KeyFrameLayerModel::columnCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent);
	return 1;
}

QModelIndex KeyFrameLayerModel::parent(const QModelIndex &index) const
{
	if(index.isValid()) {
		int seek = index.internalId();
		int right = m_items.at(seek).right;
		while(--seek >= 0) {
			if(m_items.at(seek).right > right) {
				return createIndex(m_items.at(seek).relIndex, 0, seek);
			}
		}
	}
	return QModelIndex{};
}

QModelIndex
KeyFrameLayerModel::index(int row, int column, const QModelIndex &parent) const
{
	if(m_items.isEmpty() || row < 0 || column != 0) {
		return QModelIndex{};
	}

	int cursor;
	if(parent.isValid()) {
		Q_ASSERT(m_items.at(parent.internalId()).group);
		cursor = parent.internalId();
		if(row < m_items.at(cursor).children) {
			cursor += 1; // point to the first child element
		} else {
			return QModelIndex{};
		}
	} else {
		if(row < 1) {
			cursor = 0;
		} else {
			return QModelIndex{};
		}
	}

	int next = m_items.at(cursor).right + 1;
	int i = 0;
	while(i < row) {
		while(cursor < m_items.size() && m_items.at(cursor).left < next) {
			++cursor;
		}

		if(cursor == m_items.size() || m_items.at(cursor).left > next) {
			return QModelIndex{};
		}

		next = m_items.at(cursor).right + 1;
		++i;
	}

	Q_ASSERT(m_items.at(cursor).relIndex == row);
	return createIndex(row, column, cursor);
}

QHash<int, bool> KeyFrameLayerModel::layerVisibility() const
{
	QHash<int, bool> lv;
	for(const KeyFrameLayerItem &item : m_items) {
		if(item.visibility == KeyFrameLayerItem::Visibility::Hidden) {
			lv.insert(item.id, false);
		} else if(item.visibility == KeyFrameLayerItem::Visibility::Revealed) {
			lv.insert(item.id, true);
		}
	}
	return lv;
}

void KeyFrameLayerModel::toggleVisibility(int layerId, bool revealed)
{
	KeyFrameLayerItem::Visibility vis =
		revealed ? KeyFrameLayerItem::Visibility::Revealed
				 : KeyFrameLayerItem::Visibility::Hidden;
	for(int i = 0; i < m_items.size(); ++i) {
		KeyFrameLayerItem &item = m_items[i];
		if(item.id == layerId) {
			item.visibility = item.visibility == vis
								  ? KeyFrameLayerItem::Visibility::Default
								  : vis;
			QModelIndex index = createIndex(item.relIndex, 0, i);
			dataChanged(index, index);
			dataChangedRecursive(index);
			break;
		}
	}
}

void KeyFrameLayerModel::dataChangedRecursive(QModelIndex parent)
{
	int rows = rowCount(parent);
	if(rows > 0) {
		dataChanged(index(0, 0, parent), index(rows - 1, 0, parent));
		for(int i = 0; i < rows; ++i) {
			dataChangedRecursive(index(i, 0, parent));
		}
	}
}
