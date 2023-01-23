/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#ifndef DP_ACTIONBUILDER_H
#define DP_ACTIONBUILDER_H

#include <QAction>
#include "libclient/utils/icon.h"
#include "libclient/utils/customshortcutmodel.h"

/**
 * @brief A helper class for configuring QActions
 */
class ActionBuilder
{
public:
	explicit ActionBuilder(QAction *action) : m_action(action) { Q_ASSERT(m_action); }

	operator QAction*() {
		// If an action is tagged as "remembered", it should be checkable as well
		Q_ASSERT(m_action->isCheckable() || !m_action->property("remembered").toBool());

		return m_action;
	}

	ActionBuilder &icon(const QString &name)
	{
		m_action->setIcon(icon::fromTheme(name));
		return *this;
	}

	ActionBuilder &noDefaultShortcut() {
		CustomShortcutModel::registerCustomizableAction(m_action->objectName(), m_action->text().remove('&'), QKeySequence());
		return *this;
	}

	ActionBuilder &shortcut(const QString &key) { return shortcut(QKeySequence(key)); }

	ActionBuilder &shortcut(const QKeySequence &shortcut)
	{
		Q_ASSERT(!m_action->objectName().isEmpty());
		m_action->setShortcut(shortcut);
		CustomShortcutModel::registerCustomizableAction(m_action->objectName(), m_action->text().remove('&'), shortcut);
		return *this;
	}

	ActionBuilder &checkable()
	{
		m_action->setCheckable(true);
		return *this;
	}

	ActionBuilder &checked()
	{
		m_action->setCheckable(true);
		m_action->setChecked(true);
		m_action->setProperty("defaultValue", true);
		return *this;
	}

	ActionBuilder &statusTip(const QString &tip)
	{
		m_action->setStatusTip(tip);
		return *this;
	}

	ActionBuilder &disabled()
	{
		m_action->setEnabled(false);
		return *this;
	}

	ActionBuilder &menuRole(QAction::MenuRole role)
	{
		m_action->setMenuRole(role);
		return *this;
	}

	ActionBuilder &property(const char *name, const QVariant &value)
	{
		m_action->setProperty(name, value);
		return *this;
	}

	ActionBuilder &remembered()
	{
		// Tag this (checkable) action so that its state will be
		// saved and loaded.
		Q_ASSERT(!m_action->objectName().isEmpty());
		m_action->setProperty("remembered", true);
		return *this;
	}

private:
	QAction *m_action;
};

#endif // ACTIONBUILDER_H
