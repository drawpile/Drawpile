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
#ifndef BRUSHES_CLASSICBRUSHPAINTER_H
#define BRUSHES_CLASSICBRUSHPAINTER_H

#include <Qt>

class QPointF;

namespace paintcore {
	class Layer;
	struct BrushStamp;
}

namespace protocol {
	class DrawDabsClassic;
}

namespace brushes {

/**
 * Draw brush drabs on the canvas
 */
void drawClassicBrushDabs(const protocol::DrawDabsClassic &dabs, paintcore::Layer *layer, int sublayer=0);

paintcore::BrushStamp makeGimpStyleBrushStamp(const QPointF &point, qreal radius, qreal hardness, qreal opacity);

}

#endif
