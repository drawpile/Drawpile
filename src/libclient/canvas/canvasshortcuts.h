// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CANVASSHORTCUTS_H
#define CANVASSHORTCUTS_H

#include <QSet>
#include <QVariantMap>
#include <QVector>
#include <Qt>

class QWheelEvent;

class CanvasShortcuts {
public:
	enum Type : unsigned int {
		NO_TYPE,
		KEY_COMBINATION,
		MOUSE_BUTTON,
		MOUSE_WHEEL,
		CONSTRAINT_KEY_COMBINATION,
		TYPE_COUNT,
	};

	enum Action : unsigned int {
		NO_ACTION,
		CANVAS_PAN,
		CANVAS_ROTATE,
		CANVAS_ZOOM,
		COLOR_PICK,
		LAYER_PICK,
		TOOL_ADJUST,
		CONSTRAINT,
		ACTION_COUNT,
	};

	enum Flag : unsigned int {
		NORMAL = 0x0,
		INVERTED = 0x1,
		SWAP_AXES = 0x2,
		TOOL_CONSTRAINT1 = 0x4,
		TOOL_CONSTRAINT2 = 0x8,
		CONSTRAINT_MASK = TOOL_CONSTRAINT1 | TOOL_CONSTRAINT2,
	};

	struct Shortcut {
		Type type;
		Qt::KeyboardModifiers mods;
		QSet<Qt::Key> keys;
		Qt::MouseButton button;
		Action action;
		unsigned int flags;

		bool conflictsWith(const Shortcut &other) const;

		bool matches(
			Type inType, Qt::KeyboardModifiers inMods,
			const QSet<Qt::Key> &inKeys,
			Qt::MouseButton inButton = Qt::NoButton,
			const Shortcut *prevMatch = nullptr) const;

		bool isValid(bool checkAction = true) const;
		bool isUnmodifiedClick(Qt::MouseButton inButton) const;
	};

	struct Match {
		const Shortcut *shortcut;
		Action action() { return shortcut ? shortcut->action : NO_ACTION; }
		unsigned int flags() { return shortcut ? shortcut->flags : NORMAL; }
		bool inverted() { return flags() & Flag::INVERTED; }
		bool swapAxes() { return flags() & Flag::SWAP_AXES; }

		bool isUnmodifiedClick(Qt::MouseButton button)
		{
			return shortcut && shortcut->isUnmodifiedClick(button);
		}
	};

	struct ConstraintMatch {
		unsigned int flags;
		bool toolConstraint1() { return flags & Flag::TOOL_CONSTRAINT1; }
		bool toolConstraint2() { return flags & Flag::TOOL_CONSTRAINT2; }
	};

	CanvasShortcuts();

	static CanvasShortcuts load(const QVariantMap &cfg);
	void loadDefaults();
	void clear();
	[[nodiscard]] QVariantMap save() const;

	int shortcutsCount() const;
	const CanvasShortcuts::Shortcut *shortcutAt(int index) const;
	int addShortcut(const Shortcut &s);
	int editShortcut(const Shortcut &prev, const Shortcut &s);
	void removeShortcutAt(int index, int count = 1);

	const CanvasShortcuts::Shortcut *
	searchConflict(const Shortcut &s, const Shortcut *except) const;

	Match matchKeyCombination(Qt::KeyboardModifiers mods, Qt::Key key) const;

	Match matchMouseButton(
		Qt::KeyboardModifiers mods, const QSet<Qt::Key> &keys,
		Qt::MouseButton button) const;

	Match matchMouseWheel(
		Qt::KeyboardModifiers mods, const QSet<Qt::Key> &keys) const;

	ConstraintMatch matchConstraints(
		Qt::KeyboardModifiers mods, const QSet<Qt::Key> &keys) const;

private:
	static constexpr Qt::KeyboardModifiers MODS_MASK =
		Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier |
		Qt::MetaModifier;

	int searchShortcutIndex(const Shortcut &s) const;

	Match matchShortcut(
		Type type, Qt::KeyboardModifiers mods, const QSet<Qt::Key> &keys,
		Qt::MouseButton button = Qt::NoButton) const;

	static QVector<QVariantMap> shortcutsToList(const QVariant &shortcuts);

	static QVariantMap saveShortcut(const Shortcut &s);

	static Shortcut loadShortcut(const QVariantMap &cfg);

	QVector<Shortcut> m_shortcuts;
};

#endif
