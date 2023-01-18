/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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
#ifndef CANVASVIEWMODIFIERS_H
#define CANVASVIEWMODIFIERS_H

#include <Qt>

class QSettings;

struct CanvasViewShortcut {
	Qt::KeyboardModifiers modifiers;

	CanvasViewShortcut() = default;
	CanvasViewShortcut(Qt::KeyboardModifiers m) : modifiers(m) { }
	CanvasViewShortcut(Qt::KeyboardModifier m) : modifiers(m) { }
	operator Qt::KeyboardModifiers() const { return modifiers; }

	inline bool matches(Qt::KeyboardModifiers mod, bool allowNone) const {
		if(modifiers == Qt::NoModifier) {
			return allowNone && mod == modifiers;
		} else {
			return (mod & modifiers) == modifiers;
		}
	}

	inline int len() const {
		return 4 - !(modifiers & Qt::ShiftModifier)
			 - !(modifiers & Qt::ControlModifier)
			 - !(modifiers & Qt::AltModifier)
			 - !(modifiers & Qt::MetaModifier);
	}
};

/**
 * Set of modifier key canvas shortcuts
 */
struct CanvasViewShortcuts {
	/**
	 * Quick color pick mode
	 *
	 * If set to NoModifier, quick color picking is disabled
	 */
	CanvasViewShortcut colorPick = Qt::ControlModifier;

	/**
	 * Quick layer picking mode
	 *
	 * This is similar to color pick mode, except the topmost layer with an opaque
	 * pixel under the cursor is picked.
	 *
	 * If set to NoModifier, quick layer picking is disabled
	 */
	CanvasViewShortcut layerPick = Qt::ControlModifier | Qt::ShiftModifier;

	/**
	 * Modifiers to change canvas pan mode to canvas rotation mode
	 *
	 * If set to NoModifier, rotation by dragging is disabled
	 */
	CanvasViewShortcut dragRotate = Qt::AltModifier;

	/**
	 * Modifiers to change canvas pan mode to zoom mode
	 *
	 * If set to NoModifier, zoom by dragging is disabled
	 */
	CanvasViewShortcut dragZoom = Qt::ControlModifier;

	/**
	 * Modifiers to change canvas pan mode to tool quick adjust mode
	 *
	 * If set to NoModifier, quick adjust is disabled
	 */
	CanvasViewShortcut dragQuickAdjust = Qt::ShiftModifier;

	/**
	 * Modifiers to rotate the canvas using the scroll wheel
	 *
	 * Setting to NoModifier is allowed.
	 */
	CanvasViewShortcut scrollRotate = Qt::ControlModifier | Qt::ShiftModifier;

	/**
	 * Modifiers to zoom when using the scroll wheel
	 *
	 * Setting to NoModifier is allowed.
	 */
	CanvasViewShortcut scrollZoom = Qt::NoModifier;

	/**
	 * Modifiers to quick adjust tool by scrolling
	 *
	 * Setting to NoModifier is allowed.
	 */
	CanvasViewShortcut scrollQuickAdjust = Qt::ShiftModifier;

	/**
	 * Apply constraint #1 to current tool.
	 *
	 * For shape tools (rectangle, ellipse) this means "constrain to square"
	 * For the line tool, this means "constrain to 45deg angles"
	 *
	 * If set to NoModifier, constraint #1 is never applied
	 */
	CanvasViewShortcut toolConstraint1 = Qt::ShiftModifier;

	/**
	 * Apply constraint #2 to current tool.
	 *
	 * For shape tools (rectangle, ellipse, line) this is "origin at center" mode
	 * When modifying a selection, this is activates the rotation mode.
	 * When combined with #1, it activates the selection skew mode.
	 *
	 * If set to NoModifier, constraint #2 is never applied
	 */
	CanvasViewShortcut toolConstraint2 = Qt::AltModifier;

	static CanvasViewShortcuts load(const QSettings &cfg);
	void save(QSettings &cfg) const;

	/**
	 * Match the given modifier set against the given shortcuts.
	 * The index of the longest modifier combination that matches will be
	 * returned, or -1 if none match.
	 */
	template<typename ...Args>
	static int matches(Qt::KeyboardModifiers modifiers, bool allowNone, Args... shortcuts) {
		return matches(0, -1, -1, modifiers, allowNone, shortcuts...);
	}

private:
	static int matches(int idx, int longest, int longestIdx, Qt::KeyboardModifiers modifiers, bool allowNone, CanvasViewShortcut shortcut) {
		if(shortcut.matches(modifiers, allowNone) && shortcut.len() > longest)
			longestIdx = idx;
		return longestIdx;
	}
	template<typename ...Args>
	static int matches(int idx, int longest, int longestIdx, Qt::KeyboardModifiers modifiers, bool allowNone, CanvasViewShortcut first, Args... rest) {
		if(first.matches(modifiers, allowNone) && first.len() > longest) {
			longest = first.len();
			longestIdx = idx;
		}
		return matches(idx+1, longest, longestIdx, modifiers, allowNone, rest...);
	}

};

#endif // CANVASVIEWMODIFIERS_H
