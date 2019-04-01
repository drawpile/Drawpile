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

#ifndef DP_BRUSHSTATE_H
#define DP_BRUSHSTATE_H

#include "../shared/net/message.h"

namespace paintcore {
	class Point;
	class Layer;
}

namespace brushes {

/**
 * @brief An abstract base class for brush engines
 */
class BrushState {
public:
	virtual ~BrushState() { }

	/**
	 * @brief Start or continue a stroke
	 * @param sourceLayer layer to pick up color from (when smudging.) May be nullptr
	 */
	virtual void strokeTo(const paintcore::Point &p, const paintcore::Layer *sourceLayer) = 0;

	/**
	 * @brief End the active stroke
	 */
	virtual void endStroke() = 0;

	/**
	 * @brief Take the list of DrawDab* commands accumulated so far.
	 *
	 * This clears the dab buffer but does not end the stroke.
	 *
	 * @return
	 */
	virtual protocol::MessageList takeDabs() = 0;
};

}

#endif
