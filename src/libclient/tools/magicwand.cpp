// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/magicwand.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/net/client.h"
#include "libclient/tools/selection.h"

namespace tools {

class MagicWandTool::Task final : public ToolController::Task {
public:
	Task(
		MagicWandTool *tool, const QAtomicInt &cancel,
		const drawdance::CanvasState &canvasState, const QPointF &point,
		double tolerance, int sourceLayerId, int gap, int expansion,
		int featherRadius, bool continuous, int targetLayerId,
		DP_ViewMode viewMode, int activeLayerId, int activeFrameIndex)
		: m_tool(tool)
		, m_cancel(cancel)
		, m_canvasState(canvasState)
		, m_point(point)
		, m_tolerance(tolerance)
		, m_sourceLayerId(sourceLayerId)
		, m_gap(gap)
		, m_expansion(expansion)
		, m_featherRadius(featherRadius)
		, m_continuous(continuous)
		, m_targetLayerId(targetLayerId)
		, m_viewMode(viewMode)
		, m_activeLayerId(activeLayerId)
		, m_activeFrameIndex(activeFrameIndex)
	{
	}

	void run() override
	{
		m_result = m_canvasState.floodFill(
			m_point.x(), m_point.y(), Qt::black, m_tolerance, m_sourceLayerId,
			-1, m_gap, m_expansion, m_featherRadius, m_continuous, m_viewMode,
			m_activeLayerId, m_activeFrameIndex, m_cancel, m_img, m_x, m_y);
		if(m_result != DP_FLOOD_FILL_SUCCESS &&
		   m_result != DP_FLOOD_FILL_CANCELLED) {
			m_error = QString::fromUtf8(DP_error());
		}
	}

	void finished() override { m_tool->floodFillFinished(this); }

	int targetLayerId() const { return m_targetLayerId; }
	DP_FloodFillResult result() const { return m_result; }
	const QImage &img() const { return m_img; }
	int x() const { return m_x; }
	int y() const { return m_y; }
	const QString &error() const { return m_error; }

private:
	MagicWandTool *m_tool;
	const QAtomicInt &m_cancel;
	drawdance::CanvasState m_canvasState;
	QPointF m_point;
	double m_tolerance;
	int m_sourceLayerId;
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
	QString m_error;
};


MagicWandTool::MagicWandTool(ToolController &owner)
	: Tool(
		  owner, MAGICWAND, QCursor(QPixmap(":cursors/magicwand.png"), 2, 2),
		  false, false, false, false, false)
{
}

void MagicWandTool::begin(const BeginParams &params)
{
	if(params.right) {
		cancelMultipart();
	} else if(!m_running) {
		m_running = true;
		m_cancel = false;

		const ToolController::SelectionParams &selectionParams =
			m_owner.selectionParams();
		m_op = SelectionTool::resolveOp(
			params.constrain, params.center, selectionParams.defaultOp);
		int activeLayerId = m_owner.activeLayer();
		int layerId;
		switch(selectionParams.source) {
		case int(ToolController::SelectionSource::Merged):
			layerId = 0;
			break;
		case int(ToolController::SelectionSource::MergedWithoutBackground):
			layerId = -1;
			break;
		default:
			layerId = activeLayerId;
			break;
		}

		canvas::PaintEngine *paintEngine = m_owner.model()->paintEngine();
		m_owner.executeAsync(new Task(
			this, m_cancel, paintEngine->viewCanvasState(), params.point,
			selectionParams.tolerance, layerId, selectionParams.gap,
			selectionParams.expansion, selectionParams.featherRadius,
			selectionParams.continuous, activeLayerId, paintEngine->viewMode(),
			paintEngine->viewLayer(), paintEngine->viewFrame()));
	}
}

void MagicWandTool::motion(const MotionParams &params)
{
	Q_UNUSED(params);
}

void MagicWandTool::end() {}

bool MagicWandTool::isMultipart() const
{
	return m_running && !m_cancel;
}

void MagicWandTool::undoMultipart()
{
	cancelMultipart();
}

void MagicWandTool::cancelMultipart()
{
	if(m_running) {
		m_cancel = true;
	}
}

void MagicWandTool::dispose()
{
	cancelMultipart();
}

void MagicWandTool::floodFillFinished(Task *task)
{
	m_running = false;
	DP_FloodFillResult result = task->result();
	if(result == DP_FLOOD_FILL_SUCCESS) {
		const QImage &img = task->img();
		if(img.size().isEmpty()) {
			qWarning("Magic wand: filled image is empty");
		} else {
			uint8_t contextId = m_owner.model()->localUserId();
			net::MessageList msgs;
			net::makeSelectionPutMessages(
				msgs, shouldDisguiseSelectionsAsPutImage(), contextId,
				canvas::CanvasModel::MAIN_SELECTION_ID, m_op, task->x(),
				task->y(), img.width(), img.height(), img);
			if(msgs.isEmpty()) {
				qWarning("Magic wand: no messages");
			} else {
				msgs.prepend(net::makeUndoPointMessage(contextId));
				m_owner.client()->sendMessages(msgs.count(), msgs.constData());
			}
		}
	} else if(result != DP_FLOOD_FILL_CANCELLED) {
		qWarning("Magic wand failed: %s", qUtf8Printable(task->error()));
	}
}

}
