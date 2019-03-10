/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#ifndef DP_BRUSHENGINE_H
#define DP_BRUSHENGINE_H

#include "brushstate.h"
#include "classicbrushstate.h"
#include "pixelbrushstate.h"
#include "../shared/net/message.h"

namespace brushes {

/**
 * @brief An abstraction layer for brush engines
 */
class BrushEngine : public BrushState
{
public:
	BrushEngine();

	void setBrush(int contextId, int layerId, const ClassicBrush &brush);

	void strokeTo(const paintcore::Point &p, const paintcore::Layer *sourceLayer) override { Q_ASSERT(m_activeEngine); m_activeEngine->strokeTo(p, sourceLayer); }
	void endStroke() override { Q_ASSERT(m_activeEngine); m_activeEngine->endStroke(); }
	QList<protocol::MessagePtr> takeDabs() override { Q_ASSERT(m_activeEngine); return m_activeEngine->takeDabs(); }

private:
	BrushState *m_activeEngine;

	ClassicBrushState m_classic;
	PixelBrushState m_pixel;
};

}

#endif
