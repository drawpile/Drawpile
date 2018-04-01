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

#include "brushpainter.h"
#include "classicbrushpainter.h"
#include "pixelbrushpainter.h"

#include "core/layerstack.h"
#include "../shared/net/brushes.h"

namespace brushes {

void drawBrushDabs(const protocol::Message &msg, paintcore::LayerStack *layers)
{
	Q_ASSERT(layers);

	int layerId=0;
	if(msg.type() == protocol::MSG_DRAWDABS_CLASSIC)
		layerId = static_cast<const protocol::DrawDabsClassic&>(msg).layer();
	else if(msg.type() == protocol::MSG_DRAWDABS_PIXEL)
		layerId = static_cast<const protocol::DrawDabsPixel&>(msg).layer();
	else {
		qWarning("Unhandled dab type: %s", qPrintable(msg.messageName()));
		return;
	}

	paintcore::Layer *layer = layers->getLayer(layerId);
	if(!layer) {
		qWarning("drawBrushDabs(ctx=%d, layer=%d): no such layer", msg.contextId(), layerId);
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
void drawBrushDabsDirect(const protocol::Message &msg, paintcore::Layer *layer, int sublayer)
{
	Q_ASSERT(layer);

	if(msg.type() == protocol::MSG_DRAWDABS_CLASSIC)
		drawClassicBrushDabs(static_cast<const protocol::DrawDabsClassic&>(msg), layer, sublayer);
	else if(msg.type() == protocol::MSG_DRAWDABS_PIXEL)
		drawPixelBrushDabs(static_cast<const protocol::DrawDabsPixel&>(msg), layer, sublayer);
	else
		qWarning("Unhandled dab type: %s", qPrintable(msg.messageName()));
}

}
