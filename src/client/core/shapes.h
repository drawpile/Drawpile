/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#ifndef DP_PAINTCORE_SHAPES_H
#define DP_PAINTCORE_SHAPES_H

#include <QRectF>
#include "point.h"

namespace paintcore {
namespace shapes {

PointVector rectangle(const QRectF &rect);
PointVector ellipse(const QRectF &rect);

PointVector cubicBezierCurve(const QPointF p[4]);

// These are used for brush previews
PointVector sampleStroke(const QRectF &rect);
PointVector sampleBlob(const QRectF &rect);

}
}

#endif
