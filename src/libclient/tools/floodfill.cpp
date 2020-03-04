/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2017 Calle Laakkonen

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

#include "tools/toolcontroller.h"
#include "tools/floodfill.h"

#include "core/floodfill.h"
#include "canvas/canvasmodel.h"
#include "net/client.h"
#include "net/commands.h"

#include "../libshared/net/undo.h"

#include <QGuiApplication>
#include <QPixmap>

namespace tools {

FloodFill::FloodFill(ToolController &owner)
	: Tool(owner, FLOODFILL, QCursor(QPixmap(":cursors/bucket.png"), 2, 29)),
	m_tolerance(1), m_expansion(0), m_sizelimit(1000*1000), m_sampleMerged(true), m_underFill(true),
	m_eraseMode(false)
{
}

void FloodFill::begin(const paintcore::Point &point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	Q_UNUSED(right);
#if 0 // FIXME
	QColor color = owner.activeBrush().color();

	QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	paintcore::FillResult fill = paintcore::floodfill(
		owner.model()->layerStack(),
		QPoint(point.x(), point.y()),
		m_eraseMode ? QColor() : color,
		m_tolerance,
		owner.activeLayer(),
		m_sampleMerged,
		m_sizelimit
	);

	if(!fill.oversize)
		fill = paintcore::expandFill(fill, m_expansion, color);

	if(fill.image.isNull()) {
		QGuiApplication::restoreOverrideCursor();
		return;
	}

	if(fill.oversize) {
		// Oversized fill: don't draw

	} else {
		// If the target area is transparent, use the BEHIND compositing mode.
		// This results in nice smooth blending with soft outlines, when the
		// outline has different color than the fill.
		paintcore::BlendMode::Mode mode = paintcore::BlendMode::MODE_NORMAL;

		if(m_eraseMode)
			mode = paintcore::BlendMode::MODE_ERASE;
		else if(m_underFill && (fill.layerSeedColor & 0xff000000) == 0)
			mode = paintcore::BlendMode::MODE_BEHIND;

		// Flood fill is implemented using PutImage rather than a native command.
		// This has the following advantages:
		// - backward and forward compatibility: changes in the algorithm can be made freely
		// - tolerates out-of-sync canvases (shouldn't normally happen, but...)
		// - bugs don't crash/freeze other clients
		//
		// The disadvantage is increased bandwith consumption. However, this is not as bad
		// as one might think: the effective bit-depth of the bitmap is 1bpp and most fills
		// consist of large solid areas, meaning they should compress ridiculously well.
		protocol::MessageList msgs;
		msgs << protocol::MessagePtr(new protocol::UndoPoint(owner.client()->myId()));
		msgs << net::command::putQImage(owner.client()->myId(), owner.activeLayer(), fill.x, fill.y, fill.image, mode);
		owner.client()->sendMessages(msgs);
	}

	QGuiApplication::restoreOverrideCursor();
#endif
}

void FloodFill::motion(const paintcore::Point &point, bool constrain, bool center)
{
	Q_UNUSED(point);
	Q_UNUSED(constrain);
	Q_UNUSED(center);
}

void FloodFill::end()
{
}

}
