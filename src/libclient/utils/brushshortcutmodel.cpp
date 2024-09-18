// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/brushshortcutmodel.h"
#include "libclient/brushes/brushpresetmodel.h"
#include <functional>

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;


BrushShortcutModel::BrushShortcutModel(
	brushes::BrushPresetModel *presetModel, QSize iconSize, QObject *parent)
	: QAbstractTableModel(parent)
	, m_presetModel(presetModel)
	, m_iconSize(iconSize)
{
	presetModel->getShortcutEntries(
		std::bind(&BrushShortcutModel::addEntry, this, _1, _2, _3));
}

int BrushShortcutModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_entries.size();
}

int BrushShortcutModel::columnCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : int(ColumnCount);
}

QVariant BrushShortcutModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid()) {
		return QVariant();
	}

	int row = index.row();
	if(row < 0 || row > m_entries.size()) {
		return QVariant();
	}

	switch(role) {
	case Qt::DisplayRole:
	case Qt::ToolTipRole:
	case Qt::EditRole:
		switch(index.column()) {
		case int(PresetName):
			return m_entries[row].presetName;
		case int(Shortcut):
			return m_entries[row].shortcut;
		default:
			return QVariant();
		}
	case Qt::DecorationRole:
		switch(index.column()) {
		case int(PresetName):
			return m_entries[row].presetThumbnail;
		default:
			return QVariant();
		}
	case Qt::TextAlignmentRole:
		switch(index.column()) {
		case int(Shortcut):
			return Qt::AlignCenter;
		default:
			return QVariant();
		}
	case int(FilterRole): {
		const Entry &entry = m_entries[row];
		return QStringLiteral("action:%1\nshortcut:%2")
			.arg(
				entry.presetName,
				entry.shortcut.toString(QKeySequence::NativeText));
	}
	default:
		return QVariant();
	}
}

bool BrushShortcutModel::setData(
	const QModelIndex &index, const QVariant &value, int role)
{
	if(index.isValid() && role == Qt::EditRole &&
	   index.column() == int(Shortcut)) {
		int row = index.row();
		if(row >= 0 && row < m_entries.size()) {
			m_entries[row].shortcut = value.value<QKeySequence>();
			emit dataChanged(index, index);
			return true;
		}
	}
	return false;
}

Qt::ItemFlags BrushShortcutModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
	int column = index.column();
	if(column == int(Shortcut)) {
		flags |= Qt::ItemIsEditable;
	}
	return flags;
}

QVariant BrushShortcutModel::headerData(
	int section, Qt::Orientation orientation, int role) const
{
	if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch(section) {
		case int(PresetName):
			return tr("Brush");
		case int(Shortcut):
			return tr("Shortcut");
		default:
			break;
		}
	}
	return QVariant();
}

void BrushShortcutModel::addEntry(
	int presetId, const QString &presetName, const QKeySequence &shortcut)
{
	QPixmap thumbnail = m_presetModel->searchPresetThumbnail(presetId);
	if(!thumbnail.isNull() && thumbnail.size() != m_iconSize) {
		thumbnail = thumbnail.scaled(
			m_iconSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}
	m_entries.append({presetId, presetName, thumbnail, shortcut});
}
