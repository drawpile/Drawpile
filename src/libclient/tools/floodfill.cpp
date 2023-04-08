// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/floodfill.h"

#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/net/client.h"

#include <QGuiApplication>
#include <QPixmap>

namespace tools {

FloodFill::FloodFill(ToolController &owner)
	: Tool(owner, FLOODFILL, QCursor(QPixmap(":cursors/bucket.png"), 2, 29), true, true, false)
	, m_tolerance(0.01)
	, m_expansion(0)
	, m_featherRadius(0)
	, m_sizelimit(1000*1000)
	, m_sampleMerged(true)
	, m_blendMode(DP_BLEND_MODE_NORMAL)
{
}

void FloodFill::begin(const canvas::Point &point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	if(right) {
		return;
	}

	QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	canvas::CanvasModel *model = m_owner.model();
	QColor fillColor = m_blendMode == DP_BLEND_MODE_ERASE ? Qt::black : m_owner.activeBrush().qColor();
	int layerId = m_owner.activeLayer();
	int x, y;
	QImage img;
	DP_FloodFillResult result = model->paintEngine()->viewCanvasState().floodFill(
		point.x(), point.y(), fillColor, m_tolerance, layerId, m_sampleMerged,
		m_sizelimit, m_expansion, m_featherRadius, img, x, y);

	if(result == DP_FLOOD_FILL_SUCCESS) {
		uint8_t contextId = model->localUserId();
		drawdance::MessageList msgs;
		msgs.append(drawdance::Message::makeUndoPoint(contextId));
		drawdance::Message::makePutImages(msgs, contextId, layerId, m_blendMode, x, y, img);
		m_owner.client()->sendMessages(msgs.count(), msgs.constData());
	} else if(result == DP_FLOOD_FILL_SIZE_LIMIT_EXCEEDED) {
		// The flood fill failing due to an exceeded size limit is non-obvious.
		// Show a message to the user to explain the situation.
		emit m_owner.toolTip(tr("Size limit exceeded."));
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
