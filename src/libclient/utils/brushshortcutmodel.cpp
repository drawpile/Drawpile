// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/brushshortcutmodel.h"
#include <QIcon>
#include <functional>

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;


BrushShortcutModel::BrushShortcutModel(
	brushes::BrushPresetModel *presetModel, QSize iconSize, QObject *parent)
	: QAbstractTableModel(parent)
	, m_presetModel(presetModel)
	, m_iconSize(iconSize)
	, m_presets(m_presetModel->getShortcutPresets())
{
	for(brushes::ShortcutPreset &sp : m_presets) {
		if(!sp.thumbnail.isNull() && sp.thumbnail.size() != m_iconSize) {
			sp.thumbnail = sp.thumbnail.scaled(
				m_iconSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		}
	}
	updateConflictRows(false);
}

int BrushShortcutModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_presets.size();
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
	if(row < 0 || row > m_presets.size()) {
		return QVariant();
	}

	switch(role) {
	case Qt::DisplayRole:
	case Qt::ToolTipRole: {
		QString text;
		switch(index.column()) {
		case int(PresetName):
			text = m_presets[row].name;
			break;
		case int(Shortcut):
			text = m_presets[row].shortcut.toString(QKeySequence::NativeText);
			break;
		default:
			break;
		}
		if(role == Qt::ToolTipRole && m_conflictRows.contains(row)) {
			if(text.isEmpty()) {
				//: Tooltip for a keyboard shortcut conflict.
				return tr("Conflict");
			} else {
				//: Tooltip for a keyboard shortcut conflict, %1 is the name or
				//: key sequence of the shortcut in question.
				return tr("%1 (conflict)").arg(text);
			}
		} else {
			return text;
		}
	}
	case Qt::EditRole:
		switch(index.column()) {
		case int(PresetName):
			return m_presets[row].name;
		case int(Shortcut):
			return m_presets[row].shortcut;
		default:
			return QVariant();
		}
	case Qt::DecorationRole:
		switch(index.column()) {
		case int(PresetName):
			return m_presets[row].thumbnail;
		case int(Shortcut):
			if(m_conflictRows.contains(row)) {
				return QIcon::fromTheme("dialog-warning");
			} else {
				return QVariant();
			}
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
	case Qt::ForegroundRole:
		if(m_conflictRows.contains(row)) {
			return QColor(Qt::white);
		} else {
			return QVariant();
		}
	case Qt::BackgroundRole:
		if(m_conflictRows.contains(row)) {
			return QColor(0xdc3545);
		} else {
			return QVariant();
		}
	case int(FilterRole): {
		const brushes::ShortcutPreset &sp = m_presets[row];
		QString conflictMarker =
			m_conflictRows.contains(row) ? QStringLiteral("\n\1") : QString();
		return QStringLiteral("action:%1\nshortcut:%2%3")
			.arg(
				sp.name, sp.shortcut.toString(QKeySequence::NativeText),
				conflictMarker);
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
		if(row >= 0 && row < m_presets.size()) {
			brushes::ShortcutPreset &sp = m_presets[row];
			sp.shortcut = value.value<QKeySequence>();
			m_presetModel->updatePresetShortcut(sp.id, sp.shortcut);
			emit dataChanged(index, index);
			updateConflictRows(true);
			return true;
		}
	}
	return false;
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

Qt::ItemFlags BrushShortcutModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
	int column = index.column();
	if(column == int(Shortcut)) {
		flags |= Qt::ItemIsEditable;
	}
	return flags;
}

const QKeySequence &BrushShortcutModel::shortcutAt(int row)
{
	Q_ASSERT(row >= 0);
	Q_ASSERT(row < m_presets.size());
	return m_presets[row].shortcut;
}

const QVector<int> &BrushShortcutModel::tagIdsAt(int row)
{
	Q_ASSERT(row >= 0);
	Q_ASSERT(row < m_presets.size());
	return m_presets[row].tagIds;
}

QModelIndex BrushShortcutModel::indexById(int presetId, int column) const
{
	int count = m_presets.size();
	for(int i = 0; i < count; ++i) {
		if(m_presets[i].id == presetId) {
			return createIndex(i, column);
		}
	}
	return QModelIndex();
}

void BrushShortcutModel::setExternalKeySequences(
	const QSet<QKeySequence> &externalKeySequences)
{
	if(externalKeySequences != m_externalKeySequences) {
		m_externalKeySequences = externalKeySequences;
		updateConflictRows(true);
	}
}

void BrushShortcutModel::updateConflictRows(bool emitDataChanges)
{
	QSet<int> conflictRows;
	int rowCount = m_presets.size();
	for(int i = 0; i < rowCount; ++i) {
		if(m_externalKeySequences.contains(m_presets[i].shortcut)) {
			conflictRows.insert(i);
		}
	}

	if(emitDataChanges) {
		for(int row : conflictRows + m_conflictRows) {
			if(row >= 0 &&
			   (!conflictRows.contains(row) || !m_conflictRows.contains(row))) {
				emit dataChanged(
					createIndex(row, 0), createIndex(row, ColumnCount - 1));
			}
		}
	}

	m_conflictRows.swap(conflictRows);
}


BrushShortcutFilterProxyModel::BrushShortcutFilterProxyModel(
	brushes::BrushPresetTagModel *tagModel, QObject *parent)
	: QSortFilterProxyModel(parent)
	, m_tagModel(tagModel)
{
}

void BrushShortcutFilterProxyModel::setCurrentTagRow(int tagRow)
{
	if(tagRow != m_tagRow) {
		m_tagRow = tagRow;
		invalidateFilter();
	}
}

void BrushShortcutFilterProxyModel::setSearchAllTags(bool searchAllTags)
{
	if(searchAllTags != m_searchAllTags) {
		m_searchAllTags = searchAllTags;
		if(m_haveSearch) {
			invalidateFilter();
		}
	}
}

void BrushShortcutFilterProxyModel::setSearchString(const QString &searchString)
{
	QString s = searchString.trimmed();
	m_haveSearch = !s.isEmpty();
	setFilterFixedString(s);
}

bool BrushShortcutFilterProxyModel::filterAcceptsRow(
	int sourceRow, const QModelIndex &sourceParent) const
{
	if(m_searchAllTags && m_haveSearch) {
		return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
	} else {
		BrushShortcutModel *model =
			qobject_cast<BrushShortcutModel *>(sourceModel());
		return model && !sourceParent.isValid() &&
			   m_tagModel->getTagAt(m_tagRow).accepts(
				   model->tagIdsAt(sourceRow)) &&
			   QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
	}
}
