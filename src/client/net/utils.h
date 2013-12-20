#ifndef DP_NET_UTILS_H
#define DP_NET_UTILS_H
/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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

#include "../shared/net/message.h"
#include "../shared/net/pen.h"
#include "core/point.h"

namespace paintcore {
	class Brush;
}

namespace net {

//! Generate a list of PutImage commands from a QImage
QList<protocol::MessagePtr> putQImage(int ctxid, int layer, int x, int y, const QImage &image, bool blend);

//! Generate a tool change message
protocol::MessagePtr brushToToolChange(int userid, int layer, const paintcore::Brush &brush);


protocol::PenPoint pointToProtocol(const paintcore::Point &p);
protocol::PenPointVector pointsToProtocol(const paintcore::PointVector &points);

}

#endif
