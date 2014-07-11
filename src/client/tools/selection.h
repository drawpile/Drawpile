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
#ifndef TOOLS_SELECTION_H
#define TOOLS_SELECTION_H

#include "scene/selectionitem.h"
#include "tool.h"

namespace tools {

/**
 * @brief Selection tool
 *
 * This is used for selecting regions for copying & pasting.
 */
class Selection : public Tool {
public:
	Selection(ToolCollection &owner) : Tool(owner, SELECTION) {}

	void begin(const paintcore::Point& point, bool right);
	void motion(const paintcore::Point& point, bool constrain, bool center);
	void end();

	void clearSelection();

private:
	QPoint _start, _p1;
	drawingboard::SelectionItem::Handle _handle;
};

}

#endif

