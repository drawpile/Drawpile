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
#ifndef TOOLS_COLORPICKER_H
#define TOOLS_COLORPICKER_H

#include "tool.h"

namespace tools {

/**
 * \brief Color picker
 *
 * Color picker is a local tool, it does not affect the drawing board.
 */
class ColorPicker : public Tool {
public:
	ColorPicker(ToolCollection &owner) : Tool(owner, PICKER) {}

	void begin(const paintcore::Point& point, bool right);
	void motion(const paintcore::Point& point, bool constrain, bool center);
	void end();

private:
	bool _bg;
};

}

#endif

