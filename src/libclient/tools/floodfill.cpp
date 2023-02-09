/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2021 Calle Laakkonen

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

#include "canvas/canvasmodel.h"
#include "canvas/paintengine.h"
#include "net/client.h"

#include <QGuiApplication>
#include <QPixmap>

namespace tools {

FloodFill::FloodFill(ToolController &owner)
	: Tool(owner, FLOODFILL, QCursor(QPixmap(":cursors/bucket.png"), 2, 29))
	, m_tolerance(0.01)
	, m_expansion(0)
	, m_featherRadius(0)
	, m_sizelimit(1000*1000)
	, m_sampleMerged(true)
	, m_underFill(true)
	, m_eraseMode(false)
{
}

void FloodFill::begin(const canvas::Point &point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	Q_UNUSED(right);

	QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	canvas::CanvasModel *model = owner.model();
	QColor fillColor = m_eraseMode ? Qt::black : owner.activeBrush().qColor();
	int layerId = owner.activeLayer();
	int x, y;
	QImage img;
	DP_FloodFillResult result = model->paintEngine()->viewCanvasState().floodFill(
		point.x(), point.y(), fillColor, m_tolerance, layerId,
		m_sampleMerged && !m_eraseMode, m_sizelimit, m_expansion,
		m_featherRadius, img, x, y);

	if(result == DP_FLOOD_FILL_SUCCESS) {
		uint8_t contextId = model->localUserId();
		drawdance::MessageList msgs;
		msgs.append(drawdance::Message::makeUndoPoint(contextId));
		uint8_t blendMode =
			m_eraseMode ? DP_BLEND_MODE_ERASE : m_underFill ? DP_BLEND_MODE_BEHIND : DP_BLEND_MODE_NORMAL;
		drawdance::Message::makePutImages(msgs, contextId, layerId, blendMode, x, y, img);
		owner.client()->sendMessages(msgs.count(), msgs.constData());
	} else if(result == DP_FLOOD_FILL_SIZE_LIMIT_EXCEEDED) {
		// The flood fill failing due to an exceeded size limit is non-obvious.
		// Show a message to the user to explain the situation.
		emit owner.toolTip(tr("Size limit exceeded."));
	} else {
		// Other stuff is obvious errors, like trying to fill out of bounds.
		// Don't show a message in those cases.
		qWarning("Flood fill failed: %s", DP_error());
	}

	QGuiApplication::restoreOverrideCursor();
}

void FloodFill::motion(const canvas::Point &point, bool constrain, bool center)
{
	Q_UNUSED(point);
	Q_UNUSED(constrain);
	Q_UNUSED(center);
}

void FloodFill::end()
{
}

}
