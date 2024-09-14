// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/customshortcutmodel.h"
#include <QColor>
#include <QHash>

QMap<QString, CustomShortcut> CustomShortcutModel::m_customizableActions;
QSet<QString> CustomShortcutModel::m_disabledActionNames;

CustomShortcutModel::CustomShortcutModel(QObject *parent)
	: QAbstractTableModel(parent)
{
}

int CustomShortcutModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_shortcutIndexes.size();
}

int CustomShortcutModel::columnCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : ColumnCount;
}

QVariant CustomShortcutModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid()) {
		return QVariant();
	}

	int row = index.row();
	if(row < 0 || row > m_shortcutIndexes.size()) {
		return QVariant();
	}

	if(role == Qt::DisplayRole || role == Qt::EditRole) {
		const CustomShortcut &cs = m_loadedShortcuts[m_shortcutIndexes[row]];
		switch(Column(index.column())) {
		case Action:
			return cs.title;
		case CurrentShortcut:
			return cs.currentShortcut;
		case AlternateShortcut:
			return cs.alternateShortcut;
		case DefaultShortcut:
			return cs.defaultShortcut;
		default:
			return QVariant();
		}
	} else if(role == Qt::ToolTipRole) {
		if(m_conflictRows.contains(row)) {
			return tr("Conflict");
		} else {
			return QVariant();
		}
	} else if(role == Qt::ForegroundRole) {
		if(m_conflictRows.contains(row)) {
			return QColor(Qt::white);
		} else {
			return QVariant();
		}
	} else if(role == Qt::BackgroundRole) {
		if(m_conflictRows.contains(row)) {
			return QColor(0xdc3545);
		} else {
			return QVariant{};
		}
	} else {
		return QVariant();
	}
}

bool CustomShortcutModel::setData(
	const QModelIndex &index, const QVariant &value, int role)
{
	if(!index.isValid() || role != Qt::EditRole ||
	   (index.column() != CurrentShortcut &&
		index.column() != AlternateShortcut) ||
	   index.row() >= m_shortcutIndexes.size()) {
		return false;
	}

	CustomShortcut &cs = m_loadedShortcuts[m_shortcutIndexes[index.row()]];
	if(index.column() == CurrentShortcut) {
		cs.currentShortcut = value.value<QKeySequence>();
	} else {
		cs.alternateShortcut = value.value<QKeySequence>();
	}

	emit dataChanged(index, index);
	updateConflictRows();
	return true;
}

Qt::ItemFlags CustomShortcutModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
	int column = index.column();
	if(column == CurrentShortcut || column == AlternateShortcut) {
		flags |= Qt::ItemIsEditable;
	}
	return flags;
}

QVariant CustomShortcutModel::headerData(
	int section, Qt::Orientation orientation, int role) const
{
	if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch(Column(section)) {
		case Action:
			return tr("Action");
		case CurrentShortcut:
			return tr("Shortcut");
		case AlternateShortcut:
			return tr("Alternate");
		case DefaultShortcut:
			return tr("Default");
		case ColumnCount:
			break;
		}
	}
	return QVariant();
}

QVector<CustomShortcut>
CustomShortcutModel::getShortcutsMatching(const QKeySequence &keySequence)
{
	QVector<CustomShortcut> matches;
	for(int index : m_shortcutIndexes) {
		const CustomShortcut &cs = m_loadedShortcuts[index];
		if(cs.currentShortcut == keySequence ||
		   cs.alternateShortcut == keySequence) {
			matches.append(cs);
		}
	}
	return matches;
}

void CustomShortcutModel::loadShortcuts(const QVariantMap &cfg)
{
	beginResetModel();
	m_loadedShortcuts.clear();
	m_loadedShortcuts.reserve(m_customizableActions.size());

	for(CustomShortcut a : m_customizableActions) {
		Q_ASSERT(!a.name.isEmpty());
		if(cfg.contains(a.name)) {
			const QVariant v = cfg.value(a.name);
			if(v.canConvert<QKeySequence>()) {
				a.currentShortcut = v.value<QKeySequence>();
			} else if(v.canConvert<QVariantList>()) {
				QVariantList vv = v.toList();
				if(vv.size() >= 1) {
					a.currentShortcut = vv[0].value<QKeySequence>();
				}
				if(vv.size() >= 2) {
					a.alternateShortcut = vv[1].value<QKeySequence>();
				}
			}
		} else {
			a.currentShortcut = a.defaultShortcut;
			a.alternateShortcut = a.defaultAlternateShortcut;
		}

		m_loadedShortcuts.append(a);
	}

	updateShortcuts();
	updateConflictRows();
	endResetModel();
}

QVariantMap CustomShortcutModel::saveShortcuts()
{
	QVariantMap cfg;

	for(const CustomShortcut &cs : m_loadedShortcuts) {
		if(cs.currentShortcut != cs.defaultShortcut ||
		   !cs.alternateShortcut.isEmpty()) {
			if(cs.alternateShortcut.isEmpty()) {
				cfg.insert(cs.name, cs.currentShortcut);
			} else {
				cfg.insert(
					cs.name,
					QVariantList{cs.currentShortcut, cs.alternateShortcut});
			}
		}
	}

	return cfg;
}

void CustomShortcutModel::updateShortcuts()
{
	beginResetModel();
	updateShortcutsInternal();
	updateConflictRows();
	endResetModel();
}

void CustomShortcutModel::registerCustomizableAction(
	const QString &name, const QString &title,
	const QKeySequence &defaultShortcut,
	const QKeySequence &defaultAlternateShortcut)
{
	if(!m_customizableActions.contains(name)) {
		m_customizableActions.insert(
			name, CustomShortcut{
					  name, title, defaultShortcut, defaultAlternateShortcut,
					  QKeySequence(), QKeySequence()});
	}
}

void CustomShortcutModel::changeDisabledActionNames(
	const QVector<QPair<QString, bool>> &nameDisabledPairs)
{
	for(const QPair<QString, bool> &p : nameDisabledPairs) {
		if(p.second) {
			m_disabledActionNames.insert(p.first);
		} else {
			m_disabledActionNames.remove(p.first);
		}
	}
}

QList<QKeySequence>
CustomShortcutModel::getDefaultShortcuts(const QString &name)
{
	QList<QKeySequence> defaultShortcuts;
	if(m_customizableActions.contains(name)) {
		const CustomShortcut &cs = m_customizableActions[name];
		if(!cs.defaultShortcut.isEmpty()) {
			defaultShortcuts.append(cs.defaultShortcut);
		}
		if(!cs.defaultAlternateShortcut.isEmpty()) {
			defaultShortcuts.append(cs.defaultAlternateShortcut);
		}
	}
	return defaultShortcuts;
}

void CustomShortcutModel::updateShortcutsInternal()
{
	int count = m_loadedShortcuts.size();
	m_shortcutIndexes.clear();
	m_shortcutIndexes.reserve(count - m_disabledActionNames.size());
	for(int i = 0; i < count; ++i) {
		const CustomShortcut &cs = m_loadedShortcuts[i];
		if(!m_disabledActionNames.contains(cs.name)) {
			m_shortcutIndexes.append(i);
		}
	}
}

void CustomShortcutModel::updateConflictRows()
{
	QHash<QKeySequence, QSet<int>> rowsByKeySequence;
	int rowCount = m_shortcutIndexes.size();
	for(int row = 0; row < rowCount; ++row) {
		const CustomShortcut &shortcut =
			m_loadedShortcuts[m_shortcutIndexes[row]];
		if(!shortcut.currentShortcut.isEmpty()) {
			rowsByKeySequence[shortcut.currentShortcut].insert(row);
		}
		if(!shortcut.alternateShortcut.isEmpty()) {
			rowsByKeySequence[shortcut.alternateShortcut].insert(row);
		}
	}

	QSet<int> conflictRows;
	for(const QSet<int> &values : rowsByKeySequence.values()) {
		if(values.size() > 1) {
			conflictRows.unite(values);
		}
	}

	for(int row : conflictRows + m_conflictRows) {
		if(!conflictRows.contains(row) || !m_conflictRows.contains(row)) {
			emit dataChanged(
				createIndex(row, 0), createIndex(row, ColumnCount - 1),
				{Qt::ForegroundRole, Qt::BackgroundRole});
		}
	}

	m_conflictRows = conflictRows;
}
