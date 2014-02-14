/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#ifndef TOOLS_SELECTION_H
#define TOOLS_SELECTION_H

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

