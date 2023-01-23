/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2020 Calle Laakkonen

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

#include "libclient/utils/customshortcutmodel.h"

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
	// 2 - Alternate shortcut
	// 3 - Default shortcut
	return 4;
}

QVariant CustomShortcutModel::data(const QModelIndex &index, int role) const
{
	const CustomShortcut &cs = m_shortcuts.at(index.row());

	if(role == Qt::DisplayRole || role == Qt::EditRole) {
		switch(index.column()) {
		case 0: return cs.title;
		case 1: return cs.currentShortcut;
		case 2: return cs.alternateShortcut;
		case 3: return cs.defaultShortcut;
		}
	}

	return QVariant();
}

bool CustomShortcutModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if(role == Qt::EditRole && (index.column()==1 || index.column()==2)) {
		CustomShortcut &cs = m_shortcuts[index.row()];
		if(index.column()==1)
			cs.currentShortcut = value.value<QKeySequence>();
		else
			cs.alternateShortcut = value.value<QKeySequence>();
		return true;
	}

	return false;
}

Qt::ItemFlags CustomShortcutModel::flags(const QModelIndex &index) const
{
	if(index.column()==1 || index.column()==2)
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
	case 2: return tr("Alternate");
	case 3: return tr("Default");
	}
	return QVariant();
}

void CustomShortcutModel::loadShortcuts()
{
	QVector<CustomShortcut> actions;
	actions.reserve(m_customizableActions.size());

	QSettings cfg;
	cfg.beginGroup("settings/shortcuts");

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

	std::sort(actions.begin(), actions.end());

	beginResetModel();
	m_shortcuts = actions;
	endResetModel();
}

void CustomShortcutModel::saveShortcuts()
{
	QSettings cfg;

	cfg.beginGroup("settings/shortcuts");
	cfg.remove(QString());

	for(const CustomShortcut &cs : m_shortcuts) {
		if(cs.currentShortcut != cs.defaultShortcut || !cs.alternateShortcut.isEmpty()) {
			if(cs.alternateShortcut.isEmpty()) {
				cfg.setValue(cs.name, cs.currentShortcut);
			} else {
				cfg.setValue(cs.name, QVariantList() << cs.currentShortcut << cs.alternateShortcut);
			}
		}
	}
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
