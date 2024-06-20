// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/floodfill.h"
#include "libclient/canvas/blendmodes.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/net/client.h"
#include "libclient/tools/toolcontroller.h"
#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>

namespace tools {

class FloodFill::Task final : public ToolController::Task {
public:
	Task(
		FloodFill *tool, const QAtomicInt &cancel,
		const drawdance::CanvasState &canvasState, unsigned int contextId,
		const QPointF &point, const QColor &fillColor, double tolerance,
		int sourceLayerId, int size, int gap, int expansion, int featherRadius,
		Area area, DP_ViewMode viewMode, int activeLayerId,
		int activeFrameIndex)
		: m_tool{tool}
		, m_cancel{cancel}
		, m_canvasState{canvasState}
		, m_contextId(contextId)
		, m_point{point}
		, m_fillColor{fillColor}
		, m_tolerance{tolerance}
		, m_sourceLayerId{sourceLayerId}
		, m_size{size}
		, m_gap{gap}
		, m_expansion{expansion}
		, m_featherRadius{featherRadius}
		, m_area(area)
		, m_viewMode{viewMode}
		, m_activeLayerId{activeLayerId}
		, m_activeFrameIndex{activeFrameIndex}
	{
	}

	void run() override
	{
		if(m_area == Area::Selection) {
			m_result = m_canvasState.selectionFill(
				m_contextId, canvas::CanvasModel::MAIN_SELECTION_ID,
				m_fillColor, m_expansion, m_featherRadius, m_cancel, m_img, m_x,
				m_y);
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
				m_featherRadius, continuous, m_viewMode, m_activeLayerId,
				m_activeFrameIndex, m_cancel, m_img, m_x, m_y);
		}
		if(m_result != DP_FLOOD_FILL_SUCCESS &&
		   m_result != DP_FLOOD_FILL_CANCELLED) {
			m_error = QString::fromUtf8(DP_error());
		}
	}

	void finished() override { m_tool->floodFillFinished(this); }

	DP_FloodFillResult result() const { return m_result; }
	const QImage &takeImage() const { return std::move(m_img); }
	QPoint pos() const { return QPoint(m_x, m_y); }
	Area area() const { return m_area; }
	QColor color() const { return m_fillColor; }
	const QString &error() const { return m_error; }

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
};

FloodFill::FloodFill(ToolController &owner)
	: Tool(
		  owner, FLOODFILL, QCursor(QPixmap(":cursors/bucket.png"), 2, 29),
		  true, true, false, false, false)
	, m_tolerance(0.01)
	, m_expansion(0)
	, m_featherRadius(0)
	, m_size(500)
	, m_opacity(1.0)
	, m_gap{0}
	, m_source(Source::CurrentLayer)
	, m_blendMode(DP_BLEND_MODE_NORMAL)
	, m_area(Area::Continuous)
	, m_running{false}
	, m_cancel{false}
{
}

void FloodFill::begin(const BeginParams &params)
{
	if(params.right) {
		cancelMultipart();
	} else if(havePending()) {
		flushPending();
	} else if(!m_running) {
		canvas::CanvasModel *canvas = m_owner.model();
		QColor fillColor = m_blendMode == DP_BLEND_MODE_ERASE
							   ? Qt::black
							   : m_owner.foregroundColor();

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
			layerId = m_owner.activeLayer();
			break;
		}

		m_running = true;
		m_cancel = false;
		canvas::PaintEngine *paintEngine = canvas->paintEngine();
		setHandlesRightClick(true);
		emit m_owner.toolNoticeRequested(
			QCoreApplication::translate("FillSettings", "Filling…"));
		m_owner.executeAsync(new Task{
			this, m_cancel, paintEngine->viewCanvasState(),
			canvas->localUserId(), params.point, fillColor, m_tolerance,
			layerId, m_size, m_gap, m_expansion, m_featherRadius, m_area,
			paintEngine->viewMode(), paintEngine->viewLayer(),
			paintEngine->viewFrame()});
	}
}

void FloodFill::motion(const MotionParams &params)
{
	Q_UNUSED(params);
}

void FloodFill::end() {}

bool FloodFill::isMultipart() const
{
	return (m_running && !m_cancel) || havePending();
}

void FloodFill::finishMultipart()
{
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
	if(m_running) {
		m_cancel = true;
	}
}

void FloodFill::setActiveLayer(int layerId)
{
	Q_UNUSED(layerId);
	updatePendingPreview();
}

void FloodFill::setForegroundColor(const QColor &color)
{
	Q_UNUSED(color);
	updatePendingPreview();
}

ToolState FloodFill::toolState() const
{
	return havePending() ? ToolState::AwaitingConfirmation : ToolState::Normal;
}

void FloodFill::setOpacity(qreal opacity)
{
	if(opacity != m_opacity) {
		m_opacity = opacity;
		updatePendingPreview();
	}
}

void FloodFill::setBlendMode(int blendMode)
{
	if(blendMode != m_blendMode) {
		m_blendMode = blendMode;
		updatePendingPreview();
	}
}

void FloodFill::floodFillFinished(Task *task)
{
	m_running = false;
	DP_FloodFillResult result = task->result();
	if(result == DP_FLOOD_FILL_SUCCESS) {
		m_pendingImage = task->takeImage();
		if(m_pendingImage.isNull()) {
			qWarning("Flood fill failed: image is null");
		} else {
			m_pendingPos = task->pos();
			m_pendingArea = task->area();
			m_pendingColor = task->color();
		}
	} else if(result != DP_FLOOD_FILL_CANCELLED) {
		qWarning("Flood fill failed: %s", qUtf8Printable(task->error()));
		m_pendingImage = QImage();
	}
	previewPending();
	setHandlesRightClick(havePending());
	m_owner.refreshToolState();
}

void FloodFill::updatePendingPreview()
{
	if(m_owner.model() && havePending()) {
		previewPending();
	}
}

void FloodFill::previewPending()
{
	canvas::CanvasModel *canvas = m_owner.model();
	QString toolNoticeText;
	if(canvas) {
		if(havePending()) {
			int layerId = m_owner.activeLayer();
			if(layerId <= 0) {
				canvas->paintEngine()->clearFillPreview();
				toolNoticeText = QCoreApplication::translate(
					"FillSettings", "No layer selected.");
			} else {
				QModelIndex layerIndex =
					canvas->layerlist()->layerIndex(layerId);
				QString layerTitle =
					layerIndex.data(canvas::LayerListModel::TitleRole)
						.toString();
				if(layerIndex.data(canvas::LayerListModel::IsGroupRole)
					   .toBool()) {
					canvas->paintEngine()->clearFillPreview();
					toolNoticeText =
						QCoreApplication::translate(
							"FillSettings", "Can't fill layer group %1.\n"
											"Select a regular layer instead.")
							.arg(layerTitle);
				} else {
					adjustPendingImage(false);
					canvas->paintEngine()->previewFill(
						layerId, m_blendMode, m_opacity, m_pendingPos.x(),
						m_pendingPos.y(), m_pendingImage);

					QString areaText;
					switch(m_pendingArea) {
					case Area::Continuous:
						areaText = QCoreApplication::translate(
							"FillSettings", "Continuous fill");
						break;
					case Area::Similar:
						areaText = QCoreApplication::translate(
							"FillSettings", "Similar color fill");
						break;
					case Area::Selection:
						areaText = QCoreApplication::translate(
							"FillSettings", "Selection fill");
						break;
					}

					toolNoticeText =
						QCoreApplication::translate(
							"FillSettings", "%1, %2 by %3 pixels.\n"
											"%4 at %5% opacity on %6.\n"
											"Click to apply, undo to cancel.")
							.arg(
								areaText,
								QString::number(m_pendingImage.width()),
								QString::number(m_pendingImage.height()),
								canvas::blendmode::translatedName(m_blendMode),
								QString::number(qRound(m_opacity * 100.0)),
								layerTitle);
				}
			}
		} else {
			canvas->paintEngine()->clearFillPreview();
		}
	}
	emit m_owner.toolNoticeRequested(toolNoticeText);
}

void FloodFill::flushPending()
{
	if(havePending()) {
		int layerId = m_owner.activeLayer();
		canvas::CanvasModel *canvas = m_owner.model();
		bool canFill = layerId > 0 && canvas &&
					   !canvas->layerlist()
							->layerIndex(layerId)
							.data(canvas::LayerListModel::IsGroupRole)
							.toBool();
		if(canFill) {
			adjustPendingImage(true);
			net::Client *client = m_owner.client();
			uint8_t contextId = client->myId();
			net::MessageList msgs;
			net::makePutImageMessages(
				msgs, contextId, m_owner.activeLayer(), m_blendMode,
				m_pendingPos.x(), m_pendingPos.y(), m_pendingImage);
			if(!msgs.isEmpty()) {
				msgs.prepend(net::makeUndoPointMessage(contextId));
				client->sendMessages(msgs.size(), msgs.constData());
			}
		}
		disposePending();
	}
}

void FloodFill::disposePending()
{
	if(havePending()) {
		m_pendingImage = QImage();
		previewPending();
		setHandlesRightClick(false);
		m_owner.refreshToolState();
	}
}

void FloodFill::adjustPendingImage(bool adjustOpacity)
{
	QColor color = m_owner.foregroundColor();
	bool needsColorChange =
		m_blendMode != DP_BLEND_MODE_ERASE && m_pendingColor != color;
	bool needsOpacityChange = adjustOpacity && m_opacity < 1.0;
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
			painter.setOpacity(m_opacity);
			painter.fillRect(rect, Qt::black);
		}
	}
}

}
