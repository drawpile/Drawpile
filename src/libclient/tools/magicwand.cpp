// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/magicwand.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/net/client.h"
#include "libclient/tools/selection.h"
#include "libclient/utils/cursors.h"
#include <QCoreApplication>
#include <QPainter>

using utils::Cursors;

namespace tools {

class MagicWandTool::Task final : public ToolController::Task {
public:
	Task(
		MagicWandTool *tool, const QAtomicInt &cancel,
		const drawdance::CanvasState &canvasState, const QPointF &point,
		int size, double tolerance, int sourceLayerId, int gap, int expansion,
		DP_FloodFillKernel kernel, int featherRadius, bool continuous,
		int targetLayerId, DP_ViewMode viewMode, int activeLayerId,
		int activeFrameIndex, const QColor &fillColor)
		: m_tool(tool)
		, m_cancel(cancel)
		, m_canvasState(canvasState)
		, m_point(point)
		, m_size(size)
		, m_tolerance(tolerance)
		, m_sourceLayerId(sourceLayerId)
		, m_gap(gap)
		, m_expansion(expansion)
		, m_kernel(kernel)
		, m_featherRadius(featherRadius)
		, m_continuous(continuous)
		, m_targetLayerId(targetLayerId)
		, m_viewMode(viewMode)
		, m_activeLayerId(activeLayerId)
		, m_activeFrameIndex(activeFrameIndex)
		, m_fillColor(fillColor)
	{
	}

	void run() override
	{
		m_result = m_canvasState.floodFill(
			0, 0, m_point.x(), m_point.y(), m_fillColor, m_tolerance,
			m_sourceLayerId, m_size, m_continuous ? m_gap : 0, m_expansion,
			m_kernel, m_featherRadius, false, m_continuous, false, m_viewMode,
			m_activeLayerId, m_activeFrameIndex, m_cancel, m_img, m_x, m_y);
		if(m_result != DP_FLOOD_FILL_SUCCESS &&
		   m_result != DP_FLOOD_FILL_CANCELLED) {
			m_error = QString::fromUtf8(DP_error());
		}
	}

	void finished() override { m_tool->floodFillFinished(this); }

	int targetLayerId() const { return m_targetLayerId; }
	DP_FloodFillResult result() const { return m_result; }
	QImage &&takeImage() { return std::move(m_img); }
	QPoint pos() const { return QPoint(m_x, m_y); }
	const QString &error() const { return m_error; }

private:
	MagicWandTool *m_tool;
	const QAtomicInt &m_cancel;
	drawdance::CanvasState m_canvasState;
	QPointF m_point;
	int m_size;
	double m_tolerance;
	int m_sourceLayerId;
	int m_gap;
	int m_expansion;
	DP_FloodFillKernel m_kernel;
	int m_featherRadius;
	bool m_continuous;
	int m_targetLayerId;
	DP_ViewMode m_viewMode;
	int m_activeLayerId;
	int m_activeFrameIndex;
	QColor m_fillColor;
	DP_FloodFillResult m_result;
	QImage m_img;
	int m_x;
	int m_y;
	QString m_error;
};


MagicWandTool::MagicWandTool(ToolController &owner)
	: Tool(owner, MAGICWAND, Cursors::magicWand(), Capability::AllowColorPick)
{
}

void MagicWandTool::begin(const BeginParams &params)
{
	stopDragging();
	if(params.right) {
		m_held = false;
		cancelMultipart();
	} else if(!m_running) {
		if(havePending()) {
			flushPending();
		}
		m_held = true;
		m_dragDetector.begin(params.viewPos, params.deviceType);
		fillAt(params.point, params.constrain, params.center);
	}
	updateCursor(params.constrain, params.center);
}

void MagicWandTool::motion(const MotionParams &params)
{
	m_dragDetector.motion(params.viewPos);
	if(m_dragDetector.isDrag()) {
		if(m_dragging) {
			qreal delta =
				qRound((params.viewPos.x() - m_dragPrevPoint.x()) / 2.0);

			int prevDragTolerance = qRound(m_dragTolerance);
			m_dragTolerance = qBound(0.0, m_dragTolerance + delta, 255.0);
			int dragTolerance = qRound(m_dragTolerance);

			if(dragTolerance != prevDragTolerance) {
				m_dragPrevPoint = params.viewPos;
				if(m_running) {
					m_repeat = true;
					m_cancel = true;
				} else if(havePending()) {
					fillAt(m_lastPoint, params.constrain, params.center);
				}
				emit m_owner.magicWandDragChanged(true, dragTolerance);
			}
		} else {
			m_dragging = true;
			m_dragPrevPoint = params.viewPos;
			int tolerance = m_owner.selectionParams().tolerance;
			m_dragTolerance = tolerance;
			setCursor(Qt::SplitHCursor);
			emit m_owner.magicWandDragChanged(true, tolerance);
		}
	}
	updateCursor(params.constrain, params.center);
}

void MagicWandTool::modify(const ModifyParams &params)
{
	if(m_op != -1) {
		updateOp(
			params.constrain, params.center,
			m_owner.selectionParams().defaultOp);
	}
	updateCursor(params.constrain, params.center);
}

void MagicWandTool::hover(const HoverParams &params)
{
	updateCursor(params.constrain, params.center);
}

void MagicWandTool::end(const EndParams &params)
{
	m_held = false;
	stopDragging();
	updateOp(
		params.constrain, params.center, m_owner.selectionParams().defaultOp);
	if(!m_running && !m_repeat) {
		flushPending();
	}
	updateCursor(params.constrain, params.center);
}

bool MagicWandTool::isMultipart() const
{
	return (m_running && !m_cancel) || havePending();
}

void MagicWandTool::finishMultipart()
{
	flushPending();
}

void MagicWandTool::undoMultipart()
{
	cancelMultipart();
}

void MagicWandTool::cancelMultipart()
{
	dispose();
	disposePending();
}

void MagicWandTool::dispose()
{
	m_repeat = false;
	if(m_running) {
		m_cancel = true;
	}
}

void MagicWandTool::updateParameters()
{
	if(havePending()) {
		const ToolController::SelectionParams &selectionParams =
			m_owner.selectionParams();
		bool needsRefill =
			selectionParams.size != m_pendingParams.size ||
			selectionParams.tolerance != m_pendingParams.tolerance ||
			selectionParams.expansion != m_pendingParams.expansion ||
			selectionParams.featherRadius != m_pendingParams.featherRadius ||
			selectionParams.gap != m_pendingParams.gap ||
			selectionParams.source != m_pendingParams.source ||
			selectionParams.kernel != m_pendingParams.kernel ||
			selectionParams.continuous != m_pendingParams.continuous;
		updateOp(m_lastConstrain, m_lastCenter, selectionParams.defaultOp);
		m_pendingParams = selectionParams;
		if(needsRefill) {
			repeatFill();
		}
	}
}

void MagicWandTool::updateOp(bool constrain, bool center, int defaultOp)
{
	m_lastConstrain = constrain;
	m_lastCenter = center;
	m_op = SelectionTool::resolveOp(constrain, center, defaultOp);
}

void MagicWandTool::updateCursor(bool constrain, bool center)
{
	setCursor(getCursor(
		m_op == -1 ? SelectionTool::resolveOp(
						 constrain, center, m_owner.selectionParams().defaultOp)
				   : m_op));
}

const QCursor &MagicWandTool::getCursor(int effectiveOp) const
{
	switch(effectiveOp) {
	case DP_MSG_SELECTION_PUT_OP_UNITE:
		return Cursors::magicWandUnite();
	case DP_MSG_SELECTION_PUT_OP_INTERSECT:
		return Cursors::magicWandIntersect();
	case DP_MSG_SELECTION_PUT_OP_EXCLUDE:
		return Cursors::magicWandExclude();
	default:
		return Cursors::magicWand();
	}
}

void MagicWandTool::stopDragging()
{
	if(m_dragging) {
		m_dragging = false;
		setCursor(utils::Cursors::magicWand());
		emit m_owner.magicWandDragChanged(false, 0);
	}
}

void MagicWandTool::fillAt(const QPointF &point, bool constrain, bool center)
{
	m_repeat = false;
	canvas::CanvasModel *canvas = m_owner.model();
	if(canvas && !m_running) {
		m_lastPoint = point;
		m_running = true;
		m_cancel = false;

		const ToolController::SelectionParams &selectionParams =
			m_owner.selectionParams();
		m_pendingParams = selectionParams;
		updateOp(constrain, center, selectionParams.defaultOp);
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

		setCapability(Capability::HandlesRightClick, true);
		requestToolNotice(
			QCoreApplication::translate("MagicWandSettings", "Selecting…"));

		canvas::PaintEngine *paintEngine = canvas->paintEngine();
		m_owner.executeAsync(new Task(
			this, m_cancel, paintEngine->viewCanvasState(), point,
			selectionParams.size,
			(m_dragging ? m_dragTolerance : selectionParams.tolerance) / 255.0,
			layerId, selectionParams.gap, selectionParams.expansion,
			DP_FloodFillKernel(selectionParams.kernel),
			selectionParams.featherRadius, selectionParams.continuous,
			activeLayerId, paintEngine->viewMode(), paintEngine->viewLayer(),
			paintEngine->viewFrame(), m_owner.selectionMaskColor()));
	}
}

void MagicWandTool::repeatFill()
{
	if(m_running) {
		m_repeat = true;
		m_cancel = true;
	} else if(havePending()) {
		fillAt(m_lastPoint, m_lastConstrain, m_lastCenter);
	}
}

void MagicWandTool::floodFillFinished(Task *task)
{
	m_running = false;
	DP_FloodFillResult result = task->result();
	if(result == DP_FLOOD_FILL_SUCCESS) {
		m_pendingImage = task->takeImage();
		if(m_pendingImage.isNull()) {
			qWarning("Magic wand failed: image is null");
		} else {
			m_pendingPos = task->pos();
			if(!EDITABLE) {
				emit m_owner.maskPreviewRequested(m_pendingPos, m_pendingImage);
			}
		}
	} else if(result != DP_FLOOD_FILL_CANCELLED) {
		qWarning("Magic wand failed: %s", qUtf8Printable(task->error()));
	}
	requestToolNotice(QString());
	setCapability(Capability::HandlesRightClick, havePending());
	if(m_repeat) {
		fillAt(m_lastPoint, m_lastConstrain, m_lastCenter);
	} else if(!m_held) {
		flushPending();
	}
}

void MagicWandTool::flushPending()
{
	if(havePending()) {
		if(EDITABLE) {
			adjustPendingImage();
		}
		net::Client *client = m_owner.client();
		uint8_t contextId = client->myId();
		net::MessageList msgs;
		net::makeSelectionPutMessages(
			msgs, contextId, canvas::CanvasModel::MAIN_SELECTION_ID, m_op,
			m_pendingPos.x(), m_pendingPos.y(), m_pendingImage.width(),
			m_pendingImage.height(), m_pendingImage);
		if(!msgs.isEmpty()) {
			msgs.prepend(net::makeUndoPointMessage(contextId));
			client->sendCommands(msgs.size(), msgs.constData());
		}
		disposePending();
	}
}

void MagicWandTool::disposePending()
{
	if(havePending()) {
		m_pendingImage = QImage();
		if(!EDITABLE) {
			emit m_owner.maskPreviewRequested(QPoint(), QImage());
		}
		setCapability(Capability::HandlesRightClick, false);
		requestToolNotice(QString());
	}
}

void MagicWandTool::adjustPendingImage()
{
	qreal opacity = m_owner.selectionParams().opacity;
	if(opacity < 1.0) {
		QPainter painter(&m_pendingImage);
		painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
		painter.setOpacity(opacity);
		painter.fillRect(m_pendingImage.rect(), Qt::black);
	}
}

}
