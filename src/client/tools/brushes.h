/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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
#ifndef TOOLS_BRUSHES_H
#define TOOLS_BRUSHES_H

#include "tool.h"

namespace tools {

//! Base class for brush type tools
class BrushBase : public Tool
{
	public:
		BrushBase(ToolCollection &owner, Type type) : Tool(owner, type) {}

		void begin(const paintcore::Point& point, bool right, float zoom);
		void motion(const paintcore::Point& point, bool constrain, bool center);
		void end();

		bool allowSmoothing() const { return true; }
};

/**
 * \brief Pen tool
 *
 * The pen tool draws non-antialiased strokes and full hardness.
 */
class Pen : public BrushBase {
	public:
		Pen(ToolCollection &owner) : BrushBase(owner, PEN) {}
};

/**
 * \brief Regular brush tool
 *
 * The normal brush draws antialised strokes.
 */
class Brush : public BrushBase {
	public:
		Brush(ToolCollection &owner) : BrushBase(owner, BRUSH) {}
};

/**
 * \brief Eraser tool
 *
 * The eraser too draws only using the eraser blending mode.
 * Eraser can draw either antialiased soft strokes or hard strokes.
 */
class Eraser : public BrushBase {
	public:
		Eraser(ToolCollection &owner) : BrushBase(owner, ERASER) {}
};

}

#endif

