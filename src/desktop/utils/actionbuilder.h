// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_UTILS_ACTIONBUILDER_H
#define DESKTOP_UTILS_ACTIONBUILDER_H
#include "libclient/utils/customshortcutmodel.h"
#include <QAction>
#include <QIcon>

/**
 * @brief A helper class for configuring QActions
 */
class ActionBuilder {
public:
	explicit ActionBuilder(QAction *action)
		: m_action(action)
	{
		Q_ASSERT(m_action);
	}

	QAction *get()
	{
		// Remembering only makes sense if there's a check state to remember.
		Q_ASSERT(
			m_action->isCheckable() ||
			!m_action->property("remembered").toBool());
		return m_action;
	}

	operator QAction *() { return get(); }

	ActionBuilder &icon(const QString &name)
	{
		m_action->setIcon(QIcon::fromTheme(name));
		CustomShortcutModel::setCustomizableActionIcon(
			m_action->objectName(), m_action->icon());
		return *this;
	}

	ActionBuilder &noDefaultShortcut(const QString &searchText = QString())
	{
		Q_ASSERT(!m_action->objectName().isEmpty());
		CustomShortcutModel::registerCustomizableAction(
			m_action->objectName(), m_action->text().remove('&'),
			m_action->icon(), QKeySequence(), QKeySequence(), searchText);
		return *this;
	}

	ActionBuilder &shortcut(const QString &key)
	{
		return shortcut(QKeySequence(key));
	}

	ActionBuilder &shortcut(
		const QKeySequence &shortcut,
		const QKeySequence &alternateShortcut = QKeySequence())
	{
		return shortcutWithSearchText(QString(), shortcut, alternateShortcut);
	}

	ActionBuilder &shortcutWithSearchText(
		const QString &searchText, const QKeySequence &shortcut,
		const QKeySequence &alternateShortcut = QKeySequence())
	{
		Q_ASSERT(!m_action->objectName().isEmpty());
		m_action->setShortcut(shortcut);
		CustomShortcutModel::registerCustomizableAction(
			m_action->objectName(), m_action->text().remove('&'),
			m_action->icon(), shortcut, alternateShortcut, searchText);
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

	ActionBuilder &autoRepeat()
	{
		m_action->setProperty("shortcutAutoRepeats", true);
		return *this;
	}

private:
	QAction *m_action;
};

#endif
