/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2019 Calle Laakkonen

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

#include "../shared/net/brushes.h"
#include "core/brushmask.h"
#include "core/layer.h"

namespace brushes {

template<typename T> static T square(T x) { return x*x; }

static paintcore::BrushMask makePixelBrushMask(int diameter, uchar opacity)
{
	const qreal radius = diameter/2.0;
	const qreal rr = square(radius);

	QVector<uchar> data(square(diameter), 0);
	uchar *ptr = data.data();

	const qreal offset = 0.5;
	for(int y=0;y<diameter;++y) {
		const qreal yy = square(y-radius+offset);
		for(int x=0;x<diameter;++x,++ptr) {
			const qreal xx = square(x-radius+offset);

			if(yy+xx <= rr)
				*ptr = opacity;
		}
	}
	return paintcore::BrushMask(diameter, data);
}

void drawPixelBrushDabs(const protocol::DrawDabsPixel &dabs, paintcore::EditableLayer layer, int sublayer)
{
	if(dabs.dabs().isEmpty()) {
		qWarning("drawPixelBrushDabs(ctx=%d, layer=%d): empty dab vector!", dabs.contextId(), dabs.layer());
		return;
	}

	auto blendmode = paintcore::BlendMode::Mode(dabs.mode());
	const QColor color = QColor::fromRgba(dabs.color());

	if(sublayer == 0 && color.alpha()>0)
		sublayer = dabs.contextId();

	if(sublayer != 0) {
		layer = layer.getEditableSubLayer(sublayer, blendmode, color.alpha() > 0 ? color.alpha() : 255);
		layer.updateChangeBounds(dabs.bounds());
		blendmode = paintcore::BlendMode::MODE_NORMAL;
	}

	paintcore::BrushMask mask;
	int lastSize = -1, lastOpacity = 0;

	int lastX = dabs.originX();
	int lastY = dabs.originY();
	for(const protocol::PixelBrushDab &d : dabs.dabs()) {
		const int nextX = lastX + d.x;
		const int nextY = lastY + d.y;

		if(d.size != lastSize|| d.opacity != lastOpacity) {
			// The mask is often reusable
			mask = makePixelBrushMask(d.size, d.opacity);
			lastSize = d.size;
			lastOpacity = d.opacity;
		}

		const int offset = d.size/2;
		layer.putBrushStamp(
			paintcore::BrushStamp { nextX-offset, nextY-offset, mask },
			color,
			blendmode
		);

		lastX = nextX;
		lastY = nextY;
	}
}

}
