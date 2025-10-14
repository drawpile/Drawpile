// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_HUDACTION_H
#define DESKTOP_SCENE_HUDACTION_H
#include <QMetaType>

struct HudAction final {
	enum class Type {
		None,
		ToggleBrush,
		ToggleTimeline,
		ToggleLayer,
		ToggleChat,
	};

	Type type = HudAction::Type::None;
	bool wasHovering = false;

	bool isValid() const { return type != Type::None; }
	void clear() { *this = HudAction(); }
};

Q_DECLARE_METATYPE(HudAction)

#endif
