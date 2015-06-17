#ifndef DP_NET_UTILS_H
#define DP_NET_UTILS_H
/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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

#include "../shared/net/message.h"
#include "../shared/net/pen.h"
#include "core/point.h"

namespace paintcore {
	class Brush;
}

namespace net {

//! Generate a list of PutImage commands from a QImage
/**
 * @brief Generate one or more PutImage command from a QImage
 *
 * Due to the 64k payload length limit, a large image may not fit inside
 * a single message. In this case, the image is recursively split into
 * small enough pieces.
 *
 * If the target coordinates are less than zero, the image is automatically cropped
 *
 * If mode is MODE_REPLACE and the image is large, it is split at tile boundaries (where possible)
 * to generate efficient PutImage commands.
 */
QList<protocol::MessagePtr> putQImage(int ctxid, int layer, int x, int y, QImage image, int mode);

//! Generate a tool change message
protocol::MessagePtr brushToToolChange(int userid, int layer, const paintcore::Brush &brush);


protocol::PenPoint pointToProtocol(const paintcore::Point &p);
protocol::PenPointVector pointsToProtocol(const paintcore::PointVector &points);

}

#endif
