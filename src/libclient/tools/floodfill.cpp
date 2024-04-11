// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/floodfill.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/net/client.h"
#include "libclient/tools/toolcontroller.h"
#include <QGuiApplication>
#include <QPixmap>

namespace tools {

class FloodFill::Task final : public ToolController::Task {
public:
	Task(
		FloodFill *tool, const QAtomicInt &cancel,
		const drawdance::CanvasState &canvasState, const QPointF &point,
		const QColor &fillColor, double tolerance, int sourceLayerId, int size,
		int gap, int expansion, int featherRadius, bool continuous,
		int targetLayerId, DP_ViewMode viewMode, int activeLayerId,
		int activeFrameIndex)
		: m_tool{tool}
		, m_cancel{cancel}
		, m_canvasState{canvasState}
		, m_point{point}
		, m_fillColor{fillColor}
		, m_tolerance{tolerance}
		, m_sourceLayerId{sourceLayerId}
		, m_size{size}
		, m_gap{gap}
		, m_expansion{expansion}
		, m_featherRadius{featherRadius}
		, m_continuous{continuous}
		, m_targetLayerId{targetLayerId}
		, m_viewMode{viewMode}
		, m_activeLayerId{activeLayerId}
		, m_activeFrameIndex{activeFrameIndex}
	{
	}

	void run() override
	{
		m_result = m_canvasState.floodFill(
			m_point.x(), m_point.y(), m_fillColor, m_tolerance, m_sourceLayerId,
			m_size, m_gap, m_expansion, m_featherRadius, m_continuous,
			m_viewMode, m_activeLayerId, m_activeFrameIndex, m_cancel, m_img,
			m_x, m_y);
	}

	void finished() override { m_tool->floodFillFinished(this); }

	int targetLayerId() const { return m_targetLayerId; }
	DP_FloodFillResult result() const { return m_result; }
	const QImage &img() const { return m_img; }
	int x() const { return m_x; }
	int y() const { return m_y; }

private:
	FloodFill *m_tool;
	const QAtomicInt &m_cancel;
	drawdance::CanvasState m_canvasState;
	QPointF m_point;
	QColor m_fillColor;
	double m_tolerance;
	int m_sourceLayerId;
	int m_size;
	int m_gap;
	int m_expansion;
	int m_featherRadius;
	bool m_continuous;
	int m_targetLayerId;
	DP_ViewMode m_viewMode;
	int m_activeLayerId;
	int m_activeFrameIndex;
	DP_FloodFillResult m_result;
	QImage m_img;
	int m_x;
	int m_y;
};

FloodFill::FloodFill(ToolController &owner)
	: Tool(
		  owner, FLOODFILL, QCursor(QPixmap(":cursors/bucket.png"), 2, 29),
		  true, true, false)
	, m_tolerance(0.01)
	, m_expansion(0)
	, m_featherRadius(0)
	, m_size(500)
	, m_gap{0}
	, m_layerId{0}
	, m_blendMode(DP_BLEND_MODE_NORMAL)
	, m_continuous(true)
	, m_running{false}
	, m_cancel{false}
{
}

void FloodFill::begin(
	const canvas::Point &point, bool right, float zoom, const QPointF &viewPos)
{
	Q_UNUSED(zoom);
	Q_UNUSED(viewPos);
	if(right) {
		cancelMultipart();
	} else if(!m_running) {
		canvas::CanvasModel *model = m_owner.model();
		QColor fillColor = m_blendMode == DP_BLEND_MODE_ERASE
							   ? Qt::black
							   : m_owner.activeBrush().qColor();
		m_running = true;
		m_cancel = false;
		canvas::PaintEngine *paintEngine = model->paintEngine();
		m_owner.executeAsync(new Task{
			this, m_cancel, paintEngine->viewCanvasState(), point, fillColor,
			m_tolerance, m_layerId, m_size, m_gap, m_expansion, m_featherRadius,
			m_continuous, m_owner.activeLayer(), paintEngine->viewMode(),
			paintEngine->viewLayer(), paintEngine->viewFrame()});
	}
}

void FloodFill::motion(
	const canvas::Point &point, bool constrain, bool center,
	const QPointF &viewPos)
{
	Q_UNUSED(point);
	Q_UNUSED(constrain);
	Q_UNUSED(center);
	Q_UNUSED(viewPos);
}

void FloodFill::end() {}

void FloodFill::cancelMultipart()
{
	if(m_running) {
		m_cancel = true;
	}
}

void FloodFill::floodFillFinished(Task *task)
{
	m_running = false;
	DP_FloodFillResult result = task->result();
	if(result == DP_FLOOD_FILL_SUCCESS) {
		uint8_t contextId = m_owner.model()->localUserId();
		net::MessageList msgs;
		msgs.append(net::makeUndoPointMessage(contextId));
		net::makePutImageMessages(
			msgs, contextId, task->targetLayerId(), m_blendMode, task->x(),
			task->y(), task->img());
		m_owner.client()->sendMessages(msgs.count(), msgs.constData());
	} else if(result != DP_FLOOD_FILL_CANCELLED) {
		qWarning("Flood fill failed: %s", DP_error());
	}
}

}
