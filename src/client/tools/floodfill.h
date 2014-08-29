/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#ifndef TOOLS_FLOODFILL_H
#define TOOLS_FLOODFILL_H

#include "tool.h"

namespace tools {

class FloodFill : public Tool
{
public:
	FloodFill(ToolCollection &owner);

	void begin(const paintcore::Point& point, bool right);
	void motion(const paintcore::Point& point, bool constrain, bool center);
	void end();
};

}

#endif
