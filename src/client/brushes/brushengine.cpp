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

#include "brushengine.h"

#include "classicbrushstate.h"
#include "pixelbrushstate.h"

namespace brushes {

BrushEngine::BrushEngine()
	: m_activeEngine(nullptr)
{
}

void BrushEngine::setBrush(int contextId, int layerId, const ClassicBrush &brush)
{
	// Select brush engine to use
	if(brush.subpixel()) {
		m_activeEngine = &m_classic;
		m_classic.setBrush(brush);
		m_classic.setContextId(contextId);
		m_classic.setLayer(layerId);

	} else {
		m_activeEngine = &m_pixel;
		m_pixel.setBrush(brush);
		m_pixel.setContextId(contextId);
		m_pixel.setLayer(layerId);
	}
}

}
