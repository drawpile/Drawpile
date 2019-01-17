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

#ifndef BRUSHES_BRUSHPAINTER_H
#define BRUSHES_BRUSHPAINTER_H

namespace protocol {
	class Message;
}

namespace paintcore {
	class EditableLayerStack;
	class EditableLayer;
}

namespace brushes {

/**
 * @brief Draw brush dabs onto the canvas
 *
 * The layer is selected automatically
 *
 * @param msg brush dab message
 * @param layers layer stack
 */
void drawBrushDabs(const protocol::Message &msg, paintcore::EditableLayerStack &layers);

/**
 * @brief Draw brush dabs onto a specific layer
 *
 * Typically, you should call drawBrushDabs instead. However, this function
 * must be called directly when you're drawing onto a preview sublayer.
 *
 * @param msg brush dab message
 * @param layer the layer to draw onto
 * @param sublayer sublayer override
 */
void drawBrushDabsDirect(const protocol::Message &msg, paintcore::EditableLayer layer, int sublayer=0);

}

#endif
