/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2021 Calle Laakkonen

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

#include "libclient/tools/tool.h"

namespace tools {

class FloodFill : public Tool
{
public:
	FloodFill(ToolController &owner);

	void begin(const canvas::Point& point, bool right, float zoom) override;
	void motion(const canvas::Point& point, bool constrain, bool center) override;
	void end() override;

	void setTolerance(qreal tolerance) { m_tolerance = tolerance; }
	void setExpansion(int expansion) { m_expansion = expansion; }
	void setSizeLimit(unsigned int limit) { m_sizelimit = qMax(100u, limit); }
	void setSampleMerged(bool sm) { m_sampleMerged = sm; }
	void setUnderFill(bool uf) { m_underFill = uf; }
	void setEraseMode(bool erase) { m_eraseMode = erase; }

private:
	qreal m_tolerance;
	int m_expansion;
	unsigned int m_sizelimit;
	bool m_sampleMerged;
	bool m_underFill;
	bool m_eraseMode;
};

}

#endif
