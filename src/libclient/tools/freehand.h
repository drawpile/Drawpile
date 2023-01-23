/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2018 Calle Laakkonen

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
#ifndef TOOLS_FREEHAND_H
#define TOOLS_FREEHAND_H

#include "libclient/tools/tool.h"
#include "libclient/drawdance/brushengine.h"
#include "libclient/drawdance/message.h"

namespace tools {

//! Freehand brush tool
class Freehand : public Tool
{
public:
	Freehand(ToolController &owner, bool isEraser);
	~Freehand();

	void begin(const canvas::Point& point, bool right, float zoom) override;
	void motion(const canvas::Point& point, bool constrain, bool center) override;
	void end() override;

	bool allowSmoothing() const override { return true; }

	void offsetActiveTool(int x, int y) override;

private:
	drawdance::BrushEngine m_brushEngine;
	bool m_drawing;
	bool m_firstPoint;
	qint64 m_lastTimestamp;
	canvas::Point m_start;
};

}

#endif

