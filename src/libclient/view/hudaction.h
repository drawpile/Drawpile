// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_VIEW_HUDACTION_H
#define LIBCLIENT_VIEW_HUDACTION_H
#include <QMetaType>
#include <QPoint>

class QAction;
class QMenu;

struct HudAction final {
	enum class Type {
		None,
		ToggleBrush,
		ToggleTimeline,
		ToggleLayer,
		ToggleChat,
		TriggerAction,
		TriggerMenu,
	};

	Type type = HudAction::Type::None;
	bool wasHovering = false;
	QAction *action = nullptr;
	QMenu *menu = nullptr;

	bool isValid() const { return type != Type::None; }
	void clear() { *this = HudAction(); }

	bool shouldRemoveHoverOnTrigger() const
	{
		switch(type) {
		case Type::TriggerAction:
			return false;
		default:
			return true;
		}
	}
};

Q_DECLARE_METATYPE(HudAction)

#endif
