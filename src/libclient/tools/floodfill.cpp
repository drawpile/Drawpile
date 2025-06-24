// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpmsg/blend_mode.h>
}
#include "libclient/canvas/blendmodes.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/net/client.h"
#include "libclient/tools/floodfill.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/utils/cursors.h"
#include <QGuiApplication>
#include <QPainter>

using utils::Cursors;

namespace tools {

class FloodFill::Task final : public ToolController::Task {
public:
	Task(
		FloodFill *tool, const QAtomicInt &cancel,
		const drawdance::CanvasState &canvasState, unsigned int contextId,
		const QPointF &point, const QColor &fillColor, double tolerance,
		int sourceLayerId, int size, int gap, int expansion,
		DP_FloodFillKernel kernel, int featherRadius, Area area,
		DP_ViewMode viewMode, int activeLayerId, int activeFrameIndex,
		bool editable, bool dragging)
		: m_tool(tool)
		, m_cancel(cancel)
		, m_canvasState(canvasState)
		, m_contextId(contextId)
		, m_point(point)
		, m_fillColor(fillColor)
		, m_tolerance(tolerance)
		, m_sourceLayerId(sourceLayerId)
		, m_size(size)
		, m_gap(gap)
		, m_expansion(expansion)
		, m_kernel(kernel)
		, m_featherRadius(featherRadius)
		, m_area(area)
		, m_viewMode(viewMode)
		, m_activeLayerId(activeLayerId)
		, m_activeFrameIndex(activeFrameIndex)
		, m_editable(editable)
		, m_dragging(dragging)
	{
	}

	void run() override
	{
		if(m_area == Area::Selection) {
			m_result = m_canvasState.selectionFill(
				m_contextId, canvas::CanvasModel::MAIN_SELECTION_ID,
				m_fillColor, m_expansion, m_kernel, m_featherRadius, false,
				m_cancel, m_img, m_x, m_y);
		} else {
			int size = m_canvasState.selectionExists(
						   m_contextId, canvas::CanvasModel::MAIN_SELECTION_ID)
						   ? -1
						   : m_size;
			bool continuous = m_area == Area::Continuous;
			m_result = m_canvasState.floodFill(
				m_contextId, canvas::CanvasModel::MAIN_SELECTION_ID,
				m_point.x(), m_point.y(), m_fillColor, m_tolerance,
				m_sourceLayerId, size, continuous ? m_gap : 0, m_expansion,
				m_kernel, m_featherRadius, false, continuous,
				!m_editable && !m_dragging, m_viewMode, m_activeLayerId,
				m_activeFrameIndex, m_cancel, m_img, m_x, m_y);
		}
		if(m_result != DP_FLOOD_FILL_SUCCESS &&
		   m_result != DP_FLOOD_FILL_CANCELLED) {
			m_error = QString::fromUtf8(DP_error());
		}
	}

	void finished() override { m_tool->floodFillFinished(this); }

	DP_FloodFillResult result() const { return m_result; }
	QImage &&takeImage() { return std::move(m_img); }
	QPoint pos() const { return QPoint(m_x, m_y); }
	Area area() const { return m_area; }
	QColor color() const { return m_fillColor; }
	const QString &error() const { return m_error; }
	bool isEditable() const { return m_editable; }

private:
	FloodFill *m_tool;
	const QAtomicInt &m_cancel;
	drawdance::CanvasState m_canvasState;
	unsigned int m_contextId;
	QPointF m_point;
	QColor m_fillColor;
	double m_tolerance;
	int m_sourceLayerId;
	int m_size;
	int m_gap;
	int m_expansion;
	DP_FloodFillKernel m_kernel;
	int m_featherRadius;
	Area m_area;
	DP_ViewMode m_viewMode;
	int m_activeLayerId;
	int m_activeFrameIndex;
	DP_FloodFillResult m_result;
	QImage m_img;
	int m_x;
	int m_y;
	QString m_error;
	const bool m_editable;
	const bool m_dragging;
};

FloodFill::FloodFill(ToolController &owner)
	: Tool(
		  owner, FLOODFILL, Cursors::bucket(),
		  Capability::AllowColorPick | Capability::AllowToolAdjust1 |
			  Capability::AllowToolAdjust2 | Capability::AllowToolAdjust3)
	, m_kernel(int(DP_FLOOD_FILL_KERNEL_ROUND))
	, m_blendMode(DP_BLEND_MODE_NORMAL)
	, m_originalBlendMode(m_blendMode)
{
}

void FloodFill::begin(const BeginParams &params)
{
	stopDragging();
	if(params.right) {
		cancelMultipart();
	} else if(!m_running) {
		bool shouldFill = true;
		if(havePending()) {
			shouldFill = !m_confirmFills;
			flushPending();
		}
		if(shouldFill) {
			m_dragDetector.begin(params.viewPos, params.deviceType);
			fillAt(params.point, m_owner.activeLayer(), m_editableFills);
		}
	}
}

void FloodFill::motion(const MotionParams &params)
{
	m_dragDetector.motion(params.viewPos);
	if(m_area != Area::Selection && m_dragDetector.isDrag()) {
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
					fillAt(m_lastPoint, lastActiveLayerId(), m_pendingEditable);
				}
				emit m_owner.floodFillDragChanged(true, dragTolerance);
			}
		} else {
			m_dragging = true;
			m_dragPrevPoint = params.viewPos;
			m_dragTolerance = m_tolerance;
			emit m_owner.floodFillDragChanged(true, m_tolerance);
		}
		updateCursor();
	}
}

void FloodFill::end(const EndParams &)
{
	stopDragging();
}

bool FloodFill::isMultipart() const
{
	return (m_running && !m_cancel) || havePending();
}

void FloodFill::finishMultipart()
{
	dispose();
	flushPending();
}

void FloodFill::undoMultipart()
{
	cancelMultipart();
}

void FloodFill::cancelMultipart()
{
	dispose();
	disposePending();
}

void FloodFill::dispose()
{
	m_repeat = false;
	if(m_running) {
		m_cancel = true;
	}
}

void FloodFill::flushPreviewedActions()
{
	if(havePending() && !m_pendingEditable) {
		flushPending();
	}
}

void FloodFill::setActiveLayer(int layerId)
{
	Q_UNUSED(layerId);
	updatePendingPreview();
}

void FloodFill::setLayerAlphaLock(bool alphaLock)
{
	Q_UNUSED(alphaLock);
	updatePendingPreview();
}

void FloodFill::setForegroundColor(const QColor &color)
{
	Q_UNUSED(color);
	updatePendingPreview();
}

void FloodFill::setParameters(
	int tolerance, int expansion, int kernel, int featherRadius, int size,
	qreal opacity, int gap, Source source, int blendMode, Area area,
	bool editableFills, bool confirmFills)
{
	bool needsUpdate = opacity != m_opacity || blendMode != m_blendMode;
	bool needsRefill = tolerance != m_tolerance || expansion != m_expansion ||
					   kernel != m_kernel || featherRadius != m_featherRadius ||
					   size != m_size || gap != m_gap || source != m_source ||
					   area != m_area;
	m_editableFills = editableFills;

	if(confirmFills != m_confirmFills) {
		m_confirmFills = confirmFills;
		updateCursor();
	}

	if(needsUpdate) {
		m_opacity = opacity;
		m_blendMode = blendMode;
		if(!needsRefill) {
			updatePendingPreview();
		}
	}

	if(needsRefill) {
		m_tolerance = tolerance;
		m_expansion = expansion;
		m_kernel = kernel;
		m_featherRadius = featherRadius;
		m_size = size;
		m_gap = gap;
		m_source = source;
		m_area = area;
		repeatFill();
	}
}

int FloodFill::lastActiveLayerId() const
{
	return m_lastActiveLayerId > 0 ? m_lastActiveLayerId
								   : m_owner.activeLayer();
}

void FloodFill::stopDragging()
{
	if(m_dragging) {
		m_dragging = false;
		updateCursor();
		emit m_owner.floodFillDragChanged(false, 0);
	}
}

void FloodFill::fillAt(const QPointF &point, int activeLayerId, bool editable)
{
	m_repeat = false;
	canvas::CanvasModel *canvas = m_owner.model();
	if(canvas && !m_running) {
		m_lastPoint = point;
		m_lastActiveLayerId = activeLayerId;
		m_originalLayerId = activeLayerId;
		m_originalOpacity = m_opacity;

		int layerId;
		switch(m_source) {
		case Source::Merged:
			layerId = 0;
			break;
		case Source::MergedWithoutBackground:
			layerId = -1;
			break;
		case Source::FillSourceLayer:
			layerId = canvas->layerlist()->fillSourceLayerId();
			if(layerId > 0) {
				break;
			}
			Q_FALLTHROUGH();
		default:
			layerId = activeLayerId;
			break;
		}

		canvas::PaintEngine *paintEngine = canvas->paintEngine();
		int blendMode = getEffectiveBlendModeForLayer(layerId, false);
		m_originalBlendMode = blendMode;

		QColor fillColor = blendModeHandlesColor(blendMode)
							   ? m_owner.foregroundColor()
							   : Qt::black;

		m_running = true;
		m_cancel = false;
		setCapability(Capability::HandlesRightClick, true);
		emitFloodFillStateChanged();
		requestToolNotice(
			QCoreApplication::translate("FillSettings", "Filling…"));
		m_owner.executeAsync(new Task(
			this, m_cancel, paintEngine->viewCanvasState(),
			canvas->localUserId(), point, fillColor,
			(m_dragging ? qRound(m_dragTolerance) : m_tolerance) / 255.0,
			layerId, m_size, m_gap, m_expansion, DP_FloodFillKernel(m_kernel),
			m_featherRadius, m_area, paintEngine->viewMode(),
			paintEngine->viewLayer(), paintEngine->viewFrame(), editable,
			m_dragging));
	}
}

void FloodFill::repeatFill()
{
	if(m_pendingEditable) {
		if(m_running) {
			m_repeat = true;
			m_cancel = true;
		} else if(havePending()) {
			fillAt(m_lastPoint, lastActiveLayerId(), true);
		}
	}
}

void FloodFill::floodFillFinished(Task *task)
{
	m_running = false;
	if(isActiveTool()) {
		DP_FloodFillResult result = task->result();
		if(result == DP_FLOOD_FILL_SUCCESS) {
			m_pendingImage = task->takeImage();
			if(m_pendingImage.isNull()) {
				qWarning("Flood fill failed: image is null");
			} else {
				m_pendingPos = task->pos();
				m_pendingArea = task->area();
				m_pendingColor = task->color();
				m_pendingEditable = task->isEditable();
				updateCursor();
			}
		} else if(result != DP_FLOOD_FILL_CANCELLED) {
			qWarning("Flood fill failed: %s", qUtf8Printable(task->error()));
			m_pendingImage = QImage();
		}
	} else {
		m_pendingImage = QImage();
		m_repeat = false;
	}
	previewPending();
	setCapability(Capability::HandlesRightClick, havePending());
	emitFloodFillStateChanged();
	if(m_repeat) {
		fillAt(m_lastPoint, lastActiveLayerId(), m_pendingEditable);
	}
}

void FloodFill::updatePendingPreview()
{
	if(m_owner.model() && havePending() && m_pendingEditable) {
		previewPending();
	}
}

void FloodFill::previewPending()
{
	canvas::CanvasModel *canvas = m_owner.model();
	QString toolNoticeText;
	if(canvas) {
		if(havePending()) {
			int layerId =
				m_pendingEditable ? lastActiveLayerId() : m_originalLayerId;
			if(layerId <= 0) {
				canvas->paintEngine()->clearFillPreview();
				if(m_pendingEditable) {
					toolNoticeText = QCoreApplication::translate(
						"FillSettings", "No layer selected.");
				} else {
					disposePending();
				}
			} else {
				QModelIndex layerIndex =
					canvas->layerlist()->layerIndex(layerId);
				QString layerTitle =
					layerIndex.data(canvas::LayerListModel::TitleRole)
						.toString();
				if(layerIndex.data(canvas::LayerListModel::IsGroupRole)
					   .toBool()) {
					canvas->paintEngine()->clearFillPreview();
					if(m_pendingEditable) {
						toolNoticeText = QCoreApplication::translate(
											 "FillSettings",
											 "Can't fill layer group %1.\n"
											 "Select a regular layer instead.")
											 .arg(layerTitle);
					} else {
						disposePending();
					}
				} else {
					adjustPendingImage(true, false);
					canvas->paintEngine()->previewFill(
						layerId, getEffectiveBlendModeForLayer(layerId),
						m_pendingEditable ? m_opacity : m_originalOpacity,
						m_pendingPos.x(), m_pendingPos.y(), m_pendingImage);
				}
			}
		} else {
			canvas->paintEngine()->clearFillPreview();
		}
	}
	requestToolNotice(toolNoticeText);
}

void FloodFill::flushPending()
{
	if(havePending()) {
		int layerId =
			m_pendingEditable ? m_owner.activeLayer() : m_originalLayerId;
		canvas::CanvasModel *canvas = m_owner.model();
		bool canFill = layerId > 0 && canvas &&
					   !canvas->layerlist()
							->layerIndex(layerId)
							.data(canvas::LayerListModel::IsGroupRole)
							.toBool();
		if(canFill) {
			adjustPendingImage(m_pendingEditable, true);
			net::Client *client = m_owner.client();
			uint8_t contextId = client->myId();
			net::MessageList msgs;
			net::makePutImageMessagesCompat(
				msgs, contextId, layerId,
				getEffectiveBlendModeForLayer(layerId), m_pendingPos.x(),
				m_pendingPos.y(), m_pendingImage, isCompatibilityMode());
			disposePending();
			if(!msgs.isEmpty()) {
				msgs.prepend(net::makeUndoPointMessage(contextId));
				client->sendCommands(msgs.size(), msgs.constData());
			}
		} else {
			disposePending();
		}
	}
}

void FloodFill::disposePending()
{
	if(havePending()) {
		m_pendingImage = QImage();
		previewPending();
		setCapability(Capability::HandlesRightClick, false);
		updateCursor();
		emitFloodFillStateChanged();
	}
}

void FloodFill::adjustPendingImage(bool adjustColor, bool adjustOpacity)
{
	QColor color = m_owner.foregroundColor();
	qreal opacity = m_pendingEditable ? m_opacity : m_originalOpacity;
	bool needsColorChange = adjustColor && blendModeHandlesColor(m_blendMode) &&
							m_pendingColor != color;
	bool needsOpacityChange = adjustOpacity && opacity < 1.0;
	if(needsColorChange || needsOpacityChange) {
		QPainter painter(&m_pendingImage);
		QRect rect = m_pendingImage.rect();
		if(needsColorChange) {
			painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
			painter.fillRect(rect, color);
			m_pendingColor = color;
		}
		if(needsOpacityChange) {
			painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
			painter.setOpacity(opacity);
			painter.fillRect(rect, Qt::black);
		}
	}
}

void FloodFill::updateCursor()
{
	if(m_dragging) {
		setCursor(Qt::SplitHCursor);
	} else if(havePending()) {
		setCursor(m_confirmFills ? Cursors::check() : Cursors::bucketCheck());
	} else {
		setCursor(Cursors::bucket());
	}
}

void FloodFill::emitFloodFillStateChanged()
{
	emit m_owner.floodFillStateChanged(m_running, havePending());
}

int FloodFill::getEffectiveBlendModeForLayer(
	int layerId, bool canUseOriginal) const
{
	int blendMode;
	if(m_pendingEditable || !canUseOriginal) {
		canvas::CanvasModel *canvas = m_owner.model();
		if(canvas && canvas->paintEngine()->isLayerAlphaLocked(layerId)) {
			blendMode = canvas::blendmode::toAlphaPreserving(m_blendMode);
		} else {
			blendMode = m_blendMode;
		}
	} else {
		blendMode = m_originalBlendMode;
	}

	if(isCompatibilityMode()) {
		return canvas::blendmode::toCompatible(blendMode);
	} else {
		return blendMode;
	}
}

bool FloodFill::blendModeHandlesColor(int blendMode)
{
	switch(blendMode) {
	case DP_BLEND_MODE_ERASE:
	case DP_BLEND_MODE_ERASE_PRESERVE:
		return false;
	default:
		return true;
	}
}

}
