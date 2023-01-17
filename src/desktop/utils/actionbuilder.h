// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_ACTIONBUILDER_H
#define DP_ACTIONBUILDER_H

#include <QAction>
#include <QIcon>
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
		m_action->setIcon(QIcon::fromTheme(name));
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
