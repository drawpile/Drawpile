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

#include "brushpainter.h"
#include "classicbrushpainter.h"
#include "pixelbrushpainter.h"

#include "core/layerstack.h"
#include "core/layer.h"
#include "../libshared/net/brushes.h"

namespace brushes {

void drawBrushDabs(const protocol::Message &msg, paintcore::EditableLayerStack &layers)
{
	auto layer = layers.getEditableLayer(msg.layer());
	if(layer.isNull()) {
		qWarning("drawBrushDabs(ctx=%d, layer=%d): no such layer", msg.contextId(), msg.layer());
		return;
	}

	drawBrushDabsDirect(msg, layer);
}

/**
 * @brief Draw brush dabs onto a specific layer
 *
 * Typically, you should call drawBrushDabs instead. However, this function
 * must be called directly when you're drawing onto a preview sublayer.
 *
 * @param msg brush dab message
 * @param layer the layer to draw onto
 */
void drawBrushDabsDirect(const protocol::Message &msg, paintcore::EditableLayer layer, int sublayer)
{
	Q_ASSERT(!layer.isNull());

	switch(msg.type()) {
	case protocol::MSG_DRAWDABS_CLASSIC:
		drawClassicBrushDabs(static_cast<const protocol::DrawDabsClassic&>(msg), layer, sublayer);
		break;
	case protocol::MSG_DRAWDABS_PIXEL:
	case protocol::MSG_DRAWDABS_PIXEL_SQUARE:
		drawPixelBrushDabs(static_cast<const protocol::DrawDabsPixel&>(msg), layer, sublayer);
		break;
	default:
		qWarning("Unhandled dab type: %s", qPrintable(msg.messageName()));
	}
}

}

