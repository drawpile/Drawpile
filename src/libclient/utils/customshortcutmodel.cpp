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

	if(role == Qt::DisplayRole || role == Qt::ToolTipRole) {
		const CustomShortcut &cs = m_loadedShortcuts[m_shortcutIndexes[row]];
		QString text;
		switch(Column(index.column())) {
		case Action:
			text = cs.title;
			break;
		case CurrentShortcut:
			text = cs.currentShortcut.toString(QKeySequence::NativeText);
			break;
		case AlternateShortcut:
			text = cs.alternateShortcut.toString(QKeySequence::NativeText);
			break;
		case DefaultShortcut:
			text = cs.defaultShortcut.toString(QKeySequence::NativeText);
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
	} else if(role == Qt::EditRole) {
		const CustomShortcut &cs = m_loadedShortcuts[m_shortcutIndexes[row]];
		QString text;
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
	} else if(role == Qt::DecorationRole) {
		switch(index.column()) {
		case int(Action):
			return m_loadedShortcuts[m_shortcutIndexes[row]].icon;
		case int(CurrentShortcut): {
			QHash<int, Conflict>::const_iterator it =
				m_conflictRows.constFind(row);
			if(it != m_conflictRows.end() && it->current) {
				return QIcon::fromTheme("dialog-warning");
			} else {
				return QVariant();
			}
		}
		case int(AlternateShortcut): {
			QHash<int, Conflict>::const_iterator it =
				m_conflictRows.constFind(row);
			if(it != m_conflictRows.end() && it->alternate) {
				return QIcon::fromTheme("dialog-warning");
			} else {
				return QVariant();
			}
		}
		default:
			return QVariant();
		}
	} else if(role == Qt::TextAlignmentRole) {
		switch(index.column()) {
		case int(CurrentShortcut):
		case int(AlternateShortcut):
		case int(DefaultShortcut):
			return Qt::AlignCenter;
		default:
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
			return QVariant();
		}
	} else if(role == int(FilterRole)) {
		const CustomShortcut &cs = m_loadedShortcuts[m_shortcutIndexes[row]];
		QString searchSuffix = cs.searchText.isEmpty()
								   ? QString()
								   : QStringLiteral(" (%1)").arg(cs.searchText);
		QString conflictMarker =
			m_conflictRows.contains(row) ? QStringLiteral("\n\1") : QString();
		return QStringLiteral(
				   "action:%1%2\nprimaryshortcut:%3\nalternateshortcut:%4\n"
				   "defaultprimaryshortcut:%5\ndefaultalternateshortcut:%6%7")
			.arg(
				cs.title, searchSuffix,
				cs.currentShortcut.toString(QKeySequence::NativeText),
				cs.alternateShortcut.toString(QKeySequence::NativeText),
				cs.defaultShortcut.toString(QKeySequence::NativeText),
				cs.defaultAlternateShortcut.toString(QKeySequence::NativeText),
				conflictMarker);
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
	updateConflictRows(true);
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

	updateShortcutsInternal();
	updateConflictRows(false);
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
	updateConflictRows(false);
	endResetModel();
}

const CustomShortcut &CustomShortcutModel::shortcutAt(int row) const
{
	Q_ASSERT(row >= 0);
	Q_ASSERT(row < m_shortcutIndexes.size());
	return m_loadedShortcuts[m_shortcutIndexes[row]];
}

void CustomShortcutModel::setExternalKeySequences(
	const QSet<QKeySequence> &externalKeySequences)
{
	if(m_externalKeySequences != externalKeySequences) {
		m_externalKeySequences = externalKeySequences;
		updateConflictRows(true);
	}
}

bool CustomShortcutModel::isCustomizableActionRegistered(const QString &name)
{
	return m_customizableActions.contains(name);
}

void CustomShortcutModel::registerCustomizableAction(
	const QString &name, const QString &title, const QIcon &icon,
	const QKeySequence &defaultShortcut,
	const QKeySequence &defaultAlternateShortcut, const QString &searchText)
{
	if(m_customizableActions.contains(name)) {
		qWarning(
			"Attempt to re-register existing shortcut %s",
			qUtf8Printable(name));
	} else {
		m_customizableActions.insert(
			name, {name, title, searchText, icon, defaultShortcut,
				   defaultAlternateShortcut, QKeySequence(), QKeySequence()});
	}
}

void CustomShortcutModel::setCustomizableActionIcon(
	const QString &name, const QIcon &icon)
{
	QMap<QString, CustomShortcut>::Iterator it =
		m_customizableActions.find(name);
	if(it != m_customizableActions.end()) {
		it->icon = icon;
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

void CustomShortcutModel::updateConflictRows(bool emitDataChanges)
{
	QHash<QKeySequence, QHash<int, Conflict>> rowsByKeySequence;
	for(const QKeySequence &ks : m_externalKeySequences) {
		rowsByKeySequence[ks].insert(-1, {false, false});
	}

	int rowCount = m_shortcutIndexes.size();
	for(int row = 0; row < rowCount; ++row) {
		const CustomShortcut &shortcut =
			m_loadedShortcuts[m_shortcutIndexes[row]];

		if(!shortcut.currentShortcut.isEmpty()) {
			rowsByKeySequence[shortcut.currentShortcut][row].current = true;
		}
		if(!shortcut.alternateShortcut.isEmpty()) {
			rowsByKeySequence[shortcut.alternateShortcut][row].alternate = true;
		}
	}

	QHash<int, Conflict> conflictRows;
	for(const QHash<int, Conflict> &values : rowsByKeySequence.values()) {
		if(values.size() > 1) {
			for(QHash<int, Conflict>::const_iterator it = values.constBegin(),
													 end = values.constEnd();
				it != end; ++it) {
				conflictRows[it.key()].mergeWith(it.value());
			}
		}
	}

	if(emitDataChanges) {
		QSet<int> keys;
		for(const QHash<int, Conflict> &hash : {conflictRows, m_conflictRows}) {
			for(QHash<int, Conflict>::key_iterator it = hash.keyBegin(),
												   end = hash.keyEnd();
				it != end; ++it) {
				int row = *it;
				if(row >= 0) {
					keys.insert(row);
				}
			}
		}

		for(int row : keys) {
			QHash<int, Conflict>::const_iterator it1, it2;
			if((it1 = conflictRows.constFind(row)) == conflictRows.constEnd() ||
			   (it2 = m_conflictRows.constFind(row)) ==
				   m_conflictRows.constEnd() ||
			   it1.value() != it2.value()) {
				emit dataChanged(
					createIndex(row, 0), createIndex(row, ColumnCount - 1));
			}
		}
	}

	m_conflictRows.swap(conflictRows);
}
