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

#include "libclient/tools/tool.h"

namespace tools {

/**
 * \brief Color picker
 *
 * Color picker is a local tool, it does not affect the drawing board.
 */
class ColorPicker final : public Tool {
public:
	ColorPicker(ToolController &owner);

	void begin(const canvas::Point& point, bool right, float zoom) override;
	void motion(const canvas::Point& point, bool constrain, bool center) override;
	void end() override;

	//! Pick from the current active layer only?
	void setPickFromCurrentLayer(bool p) { m_pickFromCurrentLayer = p; }

	//! Set picker size (color is averaged from the area)
	void setSize(int size) { m_size = size; }

private:
	bool m_pickFromCurrentLayer;
	int m_size;
};

}

#endif

