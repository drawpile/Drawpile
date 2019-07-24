/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018-2019 Calle Laakkonen

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
#ifndef BRUSHES_PIXELBRUSHPAINTER_H
#define BRUSHES_PIXELBRUSHPAINTER_H

namespace paintcore {
	class EditableLayer;
	class BrushMask;
}

namespace protocol {
	class DrawDabsPixel;
}

namespace brushes {

/**
 * Draw brush drabs on the canvas
 */
void drawPixelBrushDabs(const protocol::DrawDabsPixel &dabs, paintcore::EditableLayer layer, int sublayer=0);

paintcore::BrushMask makeRoundPixelBrushMask(int diameter, uchar opacity);
paintcore::BrushMask makeSquarePixelBrushMask(int diameter, uchar opacity);

}

#endif
