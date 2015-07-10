/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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
#ifndef PAINTCORE_BLENDMODES_H
#define PAINTCORE_BLENDMODES_H

#include <QString>
#include <QList>
#include <QPair>

namespace paintcore {

/**
 * Blend mode metadata
 */
struct BlendMode {
	enum Flag {
		PrivateMode = 0x00, // not available for selection
		LayerMode   = 0x01, // available for use as a layer mode
		BrushMode   = 0x02, // available for use as a brush mode
		UniversalMode = 0x03, // can be used with brushes and layers
		DecrOpacity = 0x04, // this mode can decrease pixel opacity
		IncrOpacity = 0x08  // this mode can increase pixel opacity
	};
	Q_DECLARE_FLAGS(Flags, Flag)

	//! The blending mode IDs
	// Note: these IDs are part of the protocol, so they cannot be
	// reordered without breaking it. The app internal order is different.
	enum Mode {
		MODE_ERASE=0,
		MODE_NORMAL,
		MODE_MULTIPLY,
		MODE_DIVIDE,
		MODE_BURN,
		MODE_DODGE,
		MODE_DARKEN,
		MODE_LIGHTEN,
		MODE_SUBTRACT,
		MODE_ADD,
		MODE_RECOLOR,
		MODE_BEHIND,
		MODE_COLORERASE,
		MODE_REPLACE=255
	};

	const char *name;      // translatable name
	const QString svgname; // SVG style name of this blending mode
	const Mode id;         // ID as used in the protocol
	const Flags flags;     // Behaviour info

	BlendMode() : name(nullptr), id(MODE_ERASE) { }
	BlendMode(const char *n, const QString &s, Mode i, Flags f)
		: name(n), svgname(s), id(i), flags(f) { }
};

Q_DECLARE_OPERATORS_FOR_FLAGS(BlendMode::Flags)

/**
 * @brief Find the blending mode with the given protocol ID
 * @param id blend mode ID
 * @return blend mode or default if ID does not exist
 */
const BlendMode &findBlendMode(int id);

/**
 * @brief Find the blending mode based on its SVG name
 * @param svgname
 * @param found set to false if the exact match wasn't found
 * @return blend mode or default if none by such name was found
 */
const BlendMode &findBlendModeByName(const QString &svgname, bool *found);

/**
 * @brief Get list of blend mode IDs and names that have the given flags.
 *
 * The returned list is sorted into somewhat logical groupings.
 *
 * @param flags filter by these flags
 * @return list of blending mode (id, name) pairs.
 */
QList<QPair<int, QString>> getBlendModeNames(BlendMode::Flags flags);

}

#endif

