// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/canvas/canvasshortcuts.h"

#include <QDebug>
#include <QWheelEvent>

CanvasShortcuts::CanvasShortcuts()
	: m_shortcuts{}
{
}

CanvasShortcuts CanvasShortcuts::load(const QVariantMap &cfg)
{
	const QVector<QVariantMap> shortcuts = shortcutsToList(cfg.value("shortcuts"));
	CanvasShortcuts cs;
	if (shortcuts.isEmpty() && !cfg.value("defaultsloaded").toBool()) {
		cs.loadDefaults();
	} else {
		for(const auto &shortcut : shortcuts) {
			cs.addShortcut(loadShortcut(shortcut));
		}
	}
	return cs;
}

void CanvasShortcuts::loadDefaults()
{
	constexpr auto SHIFT = Qt::KeyboardModifier::ShiftModifier;
	constexpr auto CTRL = Qt::KeyboardModifier::ControlModifier;
	constexpr auto ALT = Qt::KeyboardModifier::AltModifier;
	addShortcut({
		MOUSE_BUTTON,
		{},
		{},
		Qt::MiddleButton,
		CANVAS_PAN,
		NORMAL,
	});
	addShortcut({
		MOUSE_BUTTON,
		{},
		{Qt::Key_Space},
		Qt::LeftButton,
		CANVAS_PAN,
		NORMAL,
	});
	addShortcut({
		MOUSE_BUTTON,
		{CTRL},
		{},
		Qt::MiddleButton,
		CANVAS_ZOOM,
		NORMAL,
	});
	addShortcut({
		MOUSE_BUTTON,
		{CTRL},
		{Qt::Key_Space},
		Qt::LeftButton,
		CANVAS_ZOOM,
		NORMAL,
	});
	addShortcut({
		MOUSE_BUTTON,
		{ALT},
		{},
		Qt::MiddleButton,
		CANVAS_ROTATE,
		NORMAL,
	});
	addShortcut({
		MOUSE_BUTTON,
		{ALT},
		{Qt::Key_Space},
		Qt::LeftButton,
		CANVAS_ROTATE,
		NORMAL,
	});
	addShortcut({
		MOUSE_BUTTON,
		{ALT, SHIFT},
		{},
		Qt::MiddleButton,
		CANVAS_ROTATE_DISCRETE,
		NORMAL,
	});
	addShortcut({
		MOUSE_BUTTON,
		{ALT, SHIFT},
		{Qt::Key_Space},
		Qt::LeftButton,
		CANVAS_ROTATE_DISCRETE,
		NORMAL,
	});
	addShortcut({
		MOUSE_BUTTON,
		{CTRL},
		{},
		Qt::LeftButton,
		COLOR_PICK,
		NORMAL,
	});
	addShortcut({
		MOUSE_BUTTON,
		{SHIFT | CTRL},
		{},
		Qt::LeftButton,
		LAYER_PICK,
		NORMAL,
	});
	addShortcut({
		MOUSE_BUTTON,
		{SHIFT},
		{},
		Qt::LeftButton,
		TOOL_ADJUST,
		NORMAL,
	});
	addShortcut({
		MOUSE_BUTTON,
		{SHIFT},
		{},
		Qt::MiddleButton,
		TOOL_ADJUST,
		SWAP_AXES,
	});
	addShortcut({
		MOUSE_BUTTON,
		{SHIFT},
		{Qt::Key_Space},
		Qt::LeftButton,
		TOOL_ADJUST,
		SWAP_AXES,
	});
	// Unlike other platforms, macOS has two-dimensional scrolling by default,
	// so it makes more sense to assign canvas panning to it.
	addShortcut({
		MOUSE_WHEEL,
		{},
		{},
		Qt::NoButton,
#ifdef Q_OS_MACOS
		CANVAS_PAN,
#else
		CANVAS_ZOOM,
#endif
		NORMAL,
	});
	addShortcut({
		MOUSE_WHEEL,
		{CTRL},
		{},
		Qt::NoButton,
		CANVAS_ZOOM,
		NORMAL,
	});
	addShortcut({
		MOUSE_WHEEL,
		{SHIFT | CTRL},
		{},
		Qt::NoButton,
		CANVAS_ROTATE,
		NORMAL,
	});
	addShortcut({
		MOUSE_WHEEL,
		{SHIFT},
		{},
		Qt::NoButton,
		TOOL_ADJUST,
		NORMAL,
	});
	addShortcut({
		CONSTRAINT_KEY_COMBINATION,
		{SHIFT},
		{},
		Qt::NoButton,
		CONSTRAINT,
		TOOL_CONSTRAINT1,
	});
	addShortcut({
		CONSTRAINT_KEY_COMBINATION,
		{ALT},
		{},
		Qt::NoButton,
		CONSTRAINT,
		TOOL_CONSTRAINT2,
	});
	addShortcut({
		CONSTRAINT_KEY_COMBINATION,
		{SHIFT | ALT},
		{},
		Qt::NoButton,
		CONSTRAINT,
		TOOL_CONSTRAINT1 | TOOL_CONSTRAINT2,
	});
}

void CanvasShortcuts::clear()
{
	m_shortcuts.clear();
}

QVariantMap CanvasShortcuts::save() const
{
	QVariantMap map = { {"defaultsloaded", true} };
	QVariantList shortcuts;
	int count = m_shortcuts.size();
	for(int i = 0; i < count; ++i) {
		shortcuts.append(saveShortcut(m_shortcuts[i]));
	}
	map.insert("shortcuts", shortcuts);
	return map;
}

int CanvasShortcuts::shortcutsCount() const
{
	return m_shortcuts.size();
}

const CanvasShortcuts::Shortcut *CanvasShortcuts::shortcutAt(int index) const
{
	return index >= 0 && index < m_shortcuts.size() ? &m_shortcuts[index]
													: nullptr;
}

int CanvasShortcuts::addShortcut(const Shortcut &s)
{
	if(s.isValid()) {
		int count = m_shortcuts.size();
		int i = 0;
		while(i < count) {
			if(m_shortcuts[i].conflictsWith(s)) {
				m_shortcuts.removeAt(i);
				--count;
			} else {
				++i;
			}
		}
		int index = m_shortcuts.size();
		m_shortcuts.append(s);
		return index;
	} else {
		qWarning() << "Not adding invalid canvas shortcut"
			<< s.type << s.mods << s.keys << s.button << s.action << s.flags;
		return -1;
	}
}

int CanvasShortcuts::editShortcut(const Shortcut &prev, const Shortcut &s)
{
	if(!s.isValid()) {
		qWarning("Not editing to invalid canvas shortcut");
		return -1;
	}

	int index = searchShortcutIndex(prev);
	if(index == -1) {
		qWarning("No canvas shortcut to edit found");
		return -1;
	}

	int i = 0;
	while(i < index) {
		if(m_shortcuts[i].conflictsWith(s)) {
			m_shortcuts.removeAt(i);
			--index;
		} else {
			++i;
		}
	}

	int count = m_shortcuts.size();
	i = index + 1;
	while(i < count) {
		if(m_shortcuts[i].conflictsWith(s)) {
			m_shortcuts.removeAt(i);
			--count;
		} else {
			++i;
		}
	}

	m_shortcuts[index] = s;
	return index;
}

void CanvasShortcuts::removeShortcutAt(int index, int count)
{
	m_shortcuts.remove(index, count);
}

const CanvasShortcuts::Shortcut *
CanvasShortcuts::searchConflict(const Shortcut &s, const Shortcut *except) const
{
	for(const Shortcut &candidate : m_shortcuts) {
		if((!except || &candidate != except) && s.conflictsWith(candidate)) {
			return &candidate;
		}
	}
	return nullptr;
}

CanvasShortcuts::Match CanvasShortcuts::matchKeyCombination(
	Qt::KeyboardModifiers mods, Qt::Key key) const
{
	return matchShortcut(KEY_COMBINATION, mods, {key});
}

CanvasShortcuts::Match CanvasShortcuts::matchMouseButton(
	Qt::KeyboardModifiers mods, const QSet<Qt::Key> &keys,
	Qt::MouseButton button) const
{
	return matchShortcut(MOUSE_BUTTON, mods, keys, button);
}

CanvasShortcuts::Match CanvasShortcuts::matchMouseWheel(
	Qt::KeyboardModifiers mods, const QSet<Qt::Key> &keys) const
{
	return matchShortcut(MOUSE_WHEEL, mods, keys);
}

CanvasShortcuts::ConstraintMatch CanvasShortcuts::matchConstraints(
	Qt::KeyboardModifiers mods, const QSet<Qt::Key> &keys) const
{
	ConstraintMatch match = {NORMAL};
	for(const Shortcut &s : m_shortcuts) {
		if(s.matches(CONSTRAINT_KEY_COMBINATION, mods, keys)) {
			match.flags |= s.flags;
		}
	}
	return match;
}

bool CanvasShortcuts::Shortcut::conflictsWith(const Shortcut &other) const
{
	return type == other.type && mods == other.mods && keys == other.keys &&
		   (type != MOUSE_BUTTON || button == other.button);
}

bool CanvasShortcuts::Shortcut::matches(
	Type inType, Qt::KeyboardModifiers inMods, const QSet<Qt::Key> &inKeys,
	Qt::MouseButton inButton, const Shortcut *prevMatch) const
{
	return type == inType && (mods & MODS_MASK) == (inMods & MODS_MASK) &&
		   (keys.isEmpty() || keys.intersects(inKeys)) &&
		   (type != MOUSE_BUTTON || button == inButton) &&
		   (!prevMatch || prevMatch->keys.size() < keys.size());
}

bool CanvasShortcuts::Shortcut::isValid(bool checkAction) const
{
	// Sometimes we just want to check if the inputs are valid.
	if(checkAction) {
		if(isUnmodifiedClick(Qt::LeftButton)) {
			// A plain left click would interfere with drawing.
			return false;
		} else if(type == CONSTRAINT_KEY_COMBINATION) {
			// Constraints must have the appopriate action and must have some
			// flags that actually constrain something.
			if(action != CONSTRAINT || (flags & CONSTRAINT_MASK) == 0) {
				return false;
			}
		} else {
			// Other types must be some other in-bounds action.
			bool isInvalidAction = action <= NO_ACTION ||
								   action >= ACTION_COUNT ||
								   action == CONSTRAINT;
			if(isInvalidAction) {
				return false;
			}
		}
	}
	switch(type) {
	case KEY_COMBINATION:
		// Key combinations must have a non-modifier key.
		return !keys.isEmpty();
	case MOUSE_BUTTON:
		// Mouse button shortcuts must have a mouse button.
		return button != Qt::NoButton;
	case MOUSE_WHEEL:
		// Mouse wheel shortcuts are always valid.
		return true;
	case CONSTRAINT_KEY_COMBINATION:
		// Constraints must either have a modifier or a regular key.
		return mods != Qt::NoModifier || !keys.isEmpty();
	default:
		// Other types are invalid.
		return false;
	}
}

bool CanvasShortcuts::Shortcut::isUnmodifiedClick(
	Qt::MouseButton inButton) const
{
	return type == MOUSE_BUTTON && button == inButton &&
		   mods == Qt::NoModifier && keys.isEmpty();
}

int CanvasShortcuts::searchShortcutIndex(const Shortcut &s) const
{
	for(int i = 0; i < m_shortcuts.size(); ++i) {
		if(&m_shortcuts[i] == &s) {
			return i;
		}
	}
	return -1;
}

CanvasShortcuts::Match CanvasShortcuts::matchShortcut(
	Type type, Qt::KeyboardModifiers mods, const QSet<Qt::Key> &keys,
	Qt::MouseButton button) const
{
	const Shortcut *match = nullptr;
	for(const Shortcut &s : m_shortcuts) {
		if(s.matches(type, mods, keys, button, match)) {
			match = &s;
		}
	}
	return {match};
}

QVector<QVariantMap> CanvasShortcuts::shortcutsToList(const QVariant &shortcuts)
{
	// Qt can't tell apart lists and maps in several formats, so it's just a
	// dice roll which one we actually get.
	QVector<QVariantMap> result;
	if(shortcuts.userType() == QMetaType::QVariantList) {
		for(const QVariant &shortcut : shortcuts.toList()) {
			result.append(shortcut.toMap());
		}
	} else if(shortcuts.userType() == QMetaType::QVariantMap) {
		QVariantMap map = shortcuts.toMap();
		for(int i = 1; i <= map["size"].toInt(); ++i) {
			QString key = QString::number(i);
			result.append(map[key].toMap());
		}
	} else if(!shortcuts.isNull()) {
		qWarning("Canvas shortcuts are neither a list nor a map");
	}
	return result;
}

QVariantMap CanvasShortcuts::saveShortcut(const Shortcut &s)
{
	QVariantMap cfg;
	QList<QVariant> keyVariants;
	for(Qt::Key key : s.keys) {
		keyVariants.append(key);
	}
	// Sort keys so that the saved values don't get randomized.
	std::sort(
		keyVariants.begin(), keyVariants.end(),
		[](const QVariant &a, const QVariant &b) {
			Qt::Key ka = a.value<Qt::Key>();
			Qt::Key kb = b.value<Qt::Key>();
			return ka < kb;
		});
	cfg.insert("type", s.type);
	cfg.insert("modifiers", Qt::KeyboardModifiers::Int(s.mods));
	cfg.insert("keys", keyVariants);
	cfg.insert("button", s.button);
	cfg.insert("action", s.action);
	cfg.insert("flags", s.flags);
	return cfg;
}

CanvasShortcuts::Shortcut CanvasShortcuts::loadShortcut(const QVariantMap &cfg)
{
	Shortcut s = {
		Type(cfg.value("type", NO_TYPE).toUInt()),
		Qt::KeyboardModifiers(
			cfg.value("modifiers", Qt::KeyboardModifier::NoModifier)
				.value<Qt::KeyboardModifiers::Int>()),
		{},
		Qt::MouseButton(
			cfg.value("button", Qt::NoButton).value<Qt::MouseButtons::Int>()),
		Action(cfg.value("action", NO_ACTION).toUInt()),
		cfg.value("flags", NORMAL).toUInt(),
	};
	for(const QVariant &keyVariant : cfg.value("keys").toList()) {
		s.keys.insert(Qt::Key(keyVariant.toULongLong()));
	}
	return s;
}
