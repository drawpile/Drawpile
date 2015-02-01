/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2015 Calle Laakkonen

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

#include <QPolygon>

namespace tools {

/**
 * @brief Selection tool
 *
 * This is used for selecting regions for copying & pasting.
 */
class RectangleSelection : public Tool {
public:
	RectangleSelection(ToolCollection &owner);

	void begin(const paintcore::Point& point, bool right, float zoom);
	void motion(const paintcore::Point& point, bool constrain, bool center);
	void end();

private:
	QPoint _start, _p1;
	drawingboard::SelectionItem::Handle _handle;
};

class PolygonSelection : public Tool {
public:
	PolygonSelection(ToolCollection &owner);

	void begin(const paintcore::Point& point, bool right, float zoom);
	void motion(const paintcore::Point& point, bool constrain, bool center);
	void end();

private:
	drawingboard::SelectionItem::Handle _handle;
	QPoint _start;
};

}

#endif

