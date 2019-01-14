/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2018 Calle Laakkonen

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

#include "customshortcutmodel.h"

#include <QSettings>

#include <algorithm>

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

	// Columns:
	// 0 - Action title
	// 1 - Current shortcut
	// 2 - Default shortcut
	return 3;
}

QVariant CustomShortcutModel::data(const QModelIndex &index, int role) const
{
	const CustomShortcut &cs = m_shortcuts.at(index.row());

	if(role == Qt::DisplayRole || role == Qt::EditRole) {
		switch(index.column()) {
		case 0: return cs.title;
		case 1: return cs.currentShortcut;
		case 2: return cs.defaultShortcut;
		}
	}

	return QVariant();
}

bool CustomShortcutModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if(role == Qt::EditRole && index.column()==1) {
		CustomShortcut &cs = m_shortcuts[index.row()];
		cs.currentShortcut = value.value<QKeySequence>();
		return true;
	}

	return false;
}

Qt::ItemFlags CustomShortcutModel::flags(const QModelIndex &index) const
{
	if(index.column()==1)
		return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
	else
		return Qt::NoItemFlags;
}

QVariant CustomShortcutModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(role != Qt::DisplayRole || orientation != Qt::Horizontal)
		return QVariant();

	switch(section) {
	case 0: return tr("Action");
	case 1: return tr("Shortcut");
	case 2: return tr("Default");
	}
	return QVariant();
}

void CustomShortcutModel::loadShortcuts()
{
	QList<CustomShortcut> actions;
	actions.reserve(m_customizableActions.size());

	QSettings cfg;
	cfg.beginGroup("settings/shortcuts");

	for(CustomShortcut a : m_customizableActions) {
		Q_ASSERT(!a.name.isEmpty());
		if(cfg.contains(a.name))
			a.currentShortcut = cfg.value(a.name).value<QKeySequence>();
		else
			a.currentShortcut = a.defaultShortcut;

		actions.append(a);
	}

	std::sort(actions.begin(), actions.end(),
		[](const CustomShortcut &a1, const CustomShortcut &a2) { return a1.title.compare(a2.title) < 0; }
	);

	beginResetModel();
	m_shortcuts = actions;
	endResetModel();
}

void CustomShortcutModel::saveShortcuts()
{
	QSettings cfg;

	cfg.beginGroup("settings/shortcuts");
	cfg.remove("");

	for(const CustomShortcut &cs : m_shortcuts) {
		if(cs.currentShortcut != cs.defaultShortcut)
			cfg.setValue(cs.name, cs.currentShortcut);
	}
}

void CustomShortcutModel::registerCustomizableAction(const QString &name, const QString &title, const QKeySequence &defaultShortcut)
{
	if(m_customizableActions.contains(name))
		return;

	m_customizableActions[name] = CustomShortcut(name, title, defaultShortcut);
}

bool CustomShortcutModel::hasDefaultShortcut(const QString &name)
{
	return m_customizableActions.contains(name);
}

QKeySequence CustomShortcutModel::getDefaultShortcut(const QString &name)
{
	return m_customizableActions[name].defaultShortcut;
}

QKeySequence CustomShortcutModel::getShorcut(const QString &name)
{
	QSettings cfg;
	cfg.beginGroup("settings/shortcuts");
	if(cfg.contains(name))
		return cfg.value(name).value<QKeySequence>();
	else if(m_customizableActions.contains(name))
		return m_customizableActions[name].defaultShortcut;
	else
		return QKeySequence();
}
