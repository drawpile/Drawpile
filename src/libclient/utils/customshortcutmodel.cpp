// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/customshortcutmodel.h"

QMap<QString, CustomShortcut> CustomShortcutModel::m_customizableActions;

CustomShortcutModel::CustomShortcutModel(QObject *parent)
	: QAbstractTableModel(parent)
{
}

int CustomShortcutModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_shortcuts.size();
}

int CustomShortcutModel::columnCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;

	return ColumnCount;
}

QVariant CustomShortcutModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid()
		|| (role != Qt::DisplayRole && role != Qt::EditRole)
		|| index.row() >= m_shortcuts.size()
	) {
		return QVariant();
	}

	const auto &cs = m_shortcuts.at(index.row());

	switch(Column(index.column())) {
	case Action: return cs.title;
	case CurrentShortcut: return cs.currentShortcut;
	case AlternateShortcut: return cs.alternateShortcut;
	case DefaultShortcut: return cs.defaultShortcut;
	case ColumnCount: {}
	}

	return QVariant();
}

bool CustomShortcutModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if(!index.isValid()
		|| role != Qt::EditRole
		|| (index.column() != CurrentShortcut && index.column() != AlternateShortcut)
		|| index.row() >= m_shortcuts.size()
	) {
		return false;
	}

	auto &cs = m_shortcuts[index.row()];
	if(index.column() == CurrentShortcut)
		cs.currentShortcut = value.value<QKeySequence>();
	else
		cs.alternateShortcut = value.value<QKeySequence>();

	emit dataChanged(index, index);
	return true;
}

Qt::ItemFlags CustomShortcutModel::flags(const QModelIndex &index) const
{
	auto flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
	if(index.column() == CurrentShortcut || index.column() == AlternateShortcut)
		flags |= Qt::ItemIsEditable;
	return flags;
}

QVariant CustomShortcutModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(role != Qt::DisplayRole || orientation != Qt::Horizontal)
		return QVariant();

	switch(Column(section)) {
	case Action: return tr("Action");
	case CurrentShortcut: return tr("Shortcut");
	case AlternateShortcut: return tr("Alternate");
	case DefaultShortcut: return tr("Default");
	case ColumnCount: {}
	}
	return QVariant();
}

QVector<CustomShortcut> CustomShortcutModel::getShortcutsMatching(const QKeySequence &keySequence)
{
	QVector<CustomShortcut> matches;
	for(const CustomShortcut &shortcut : m_shortcuts) {
		if(shortcut.currentShortcut == keySequence || shortcut.alternateShortcut == keySequence) {
			matches.append(shortcut);
		}
	}
	return matches;
}

void CustomShortcutModel::loadShortcuts(const QVariantMap &cfg)
{
	QVector<CustomShortcut> actions;
	actions.reserve(m_customizableActions.size());

	for(CustomShortcut a : m_customizableActions) {
		Q_ASSERT(!a.name.isEmpty());
		if(cfg.contains(a.name)) {
			const QVariant v = cfg.value(a.name);
			if(v.canConvert<QKeySequence>())
				a.currentShortcut = v.value<QKeySequence>();
			else if(v.canConvert<QVariantList>()) {
				auto vv = v.toList();
				if(vv.size() >= 1)
					a.currentShortcut = vv[0].value<QKeySequence>();
				if(vv.size() >= 2)
					a.alternateShortcut = vv[1].value<QKeySequence>();
			}
		} else {
			a.currentShortcut = a.defaultShortcut;
		}

		actions.append(a);
	}

	beginResetModel();
	m_shortcuts = actions;
	endResetModel();
}

QVariantMap CustomShortcutModel::saveShortcuts()
{
	QVariantMap cfg;

	for(const CustomShortcut &cs : m_shortcuts) {
		if(cs.currentShortcut != cs.defaultShortcut || !cs.alternateShortcut.isEmpty()) {
			if(cs.alternateShortcut.isEmpty()) {
				cfg.insert(cs.name, cs.currentShortcut);
			} else {
				cfg.insert(cs.name, QVariantList() << cs.currentShortcut << cs.alternateShortcut);
			}
		}
	}

	return cfg;
}

void CustomShortcutModel::registerCustomizableAction(const QString &name, const QString &title, const QKeySequence &defaultShortcut)
{
	if(m_customizableActions.contains(name))
		return;

	m_customizableActions[name] = CustomShortcut {
		name,
		title,
		defaultShortcut,
		QKeySequence(),
		QKeySequence()
	};
}

QKeySequence CustomShortcutModel::getDefaultShortcut(const QString &name)
{
	if(m_customizableActions.contains(name))
		return m_customizableActions[name].defaultShortcut;
	else
		return QKeySequence();
}
