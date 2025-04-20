// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpmsg/ids.h>
}
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/point.h"
#include "libclient/canvas/transformmodel.h"
#include "libclient/net/client.h"
#include "libclient/settings.h"
#include "libclient/tools/annotation.h"
#include "libclient/tools/beziertool.h"
#include "libclient/tools/colorpicker.h"
#include "libclient/tools/floodfill.h"
#include "libclient/tools/freehand.h"
#include "libclient/tools/inspector.h"
#include "libclient/tools/laser.h"
#include "libclient/tools/magicwand.h"
#include "libclient/tools/pan.h"
#include "libclient/tools/selection.h"
#include "libclient/tools/shapetools.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/transform.h"
#include "libclient/tools/zoom.h"
#include "libshared/util/functionrunnable.h"

namespace tools {

ToolController::ToolController(net::Client *client, QObject *parent)
	: QObject(parent)
	, m_toolbox{}
	, m_client(client)
	, m_model(nullptr)
	, m_activeTool(nullptr)
	, m_drawing(false)
	, m_applyGlobalSmoothing(true)
	, m_mouseSmoothing(false)
	, m_globalSmoothing(0)
	, m_interpolateInputs(false)
	, m_stabilizationMode(brushes::Stabilizer)
	, m_stabilizerSampleCount(0)
	, m_smoothing(0)
	, m_effectiveSmoothing(0)
	, m_finishStrokes(true)
	, m_stabilizerUseBrushSampleCount(true)
	, m_transformPreviewAccurate(true)
	, m_transformInterpolation{DP_MSG_TRANSFORM_REGION_MODE_BILINEAR}
	, m_transformPreviewIdsUsed(0)
	, m_threadPool{this}
	, m_taskCount{0}
{
	Q_ASSERT(client);

	registerTool(new Freehand(*this, false));
	registerTool(
		new Freehand(*this, true)); // eraser is a specialized freehand tool
	registerTool(new ColorPicker(*this));
	registerTool(new Line(*this));
	registerTool(new Rectangle(*this));
	registerTool(new Ellipse(*this));
	registerTool(new BezierTool(*this));
	registerTool(new FloodFill(*this));
	registerTool(new Annotation(*this));
	registerTool(new LaserPointer(*this));
	registerTool(new RectangleSelection(*this));
	registerTool(new PolygonSelection(*this));
	registerTool(new MagicWandTool(*this));
	registerTool(new PanTool(*this));
	registerTool(new ZoomTool(*this));
	registerTool(new Inspector(*this));
	registerTool(new TransformTool(*this));

	m_activeTool = m_toolbox[Tool::FREEHAND];
	m_activeLayer = 0;
	m_activeAnnotation = 0;

	m_threadPool.setMaxThreadCount(1);
	connect(
		this, &ToolController::asyncExecutionFinished, this,
		&ToolController::notifyAsyncExecutionFinished, Qt::QueuedConnection);
}

void ToolController::registerTool(Tool *tool)
{
	Q_ASSERT(tool->type() >= 0 && tool->type() < Tool::_LASTTOOL);
	Q_ASSERT(m_toolbox[int(tool->type())] == nullptr);
	m_toolbox[tool->type()] = tool;
}

ToolController::~ToolController()
{
	// The transform tool may want to switch back to a previous tool when it's
	// cancelled. That causes signals to be emitted into objects that are being
	// destroyed, which Qt doesn't appreciate. So we block signals here.
	blockSignals(true);
	// May be null if a file failed to open or a session didn't connect.
	if(m_model) {
		for(Tool *t : m_toolbox) {
			t->dispose();
		}
	}
	m_threadPool.waitForDone();
	for(Tool *t : m_toolbox) {
		delete t;
	}
}

Tool *ToolController::getTool(Tool::Type type)
{
	Q_ASSERT(type >= 0 && type < Tool::_LASTTOOL);
	Tool *t = m_toolbox[type];
	Q_ASSERT(t);
	return t;
}

TransformTool *ToolController::transformTool()
{
	return static_cast<TransformTool *>(m_toolbox[Tool::TRANSFORM]);
}

void ToolController::finishActiveTool()
{
	endDrawing(false, false);
	if(m_activeTool->isMultipart()) {
		m_activeTool->finishMultipart();
		if(m_activeTool->isMultipart()) {
			m_activeTool->cancelMultipart();
		}
	}
}

void ToolController::setActiveTool(Tool::Type tool)
{
	if(activeTool() != tool) {
		finishActiveTool();
		m_activeTool = getTool(tool);
		emit toolCapabilitiesChanged(activeToolCapabilities());
		emit toolCursorChanged(activeToolCursor());
		emit toolNoticeRequested(QString());
	}
}

void ToolController::setActiveAnnotation(int id)
{
	if(m_activeAnnotation != id) {
		if(m_activeAnnotation != 0) {
			drawdance::Annotation a =
				m_model->paintEngine()->getAnnotationById(m_activeAnnotation);
			if(!a.isNull() && a.textBytes().isEmpty()) {
				emit deleteAnnotationRequested(a.id());
			}
		}

		m_activeAnnotation = id;
		emit activeAnnotationChanged(id);
	}
}

void ToolController::setSelectionEditActive(bool selectionEditActive)
{
	if(selectionEditActive != m_selectionEditActive) {
		m_selectionEditActive = selectionEditActive;
		emit selectionEditActiveChanged(selectionEditActive);
	}
}

int ToolController::activeLayerOrSelection() const
{
	if(m_selectionEditActive) {
		return DP_selection_id_make(
			m_client->myId(), canvas::CanvasModel::MAIN_SELECTION_ID);
	} else {
		return m_activeLayer;
	}
}

void ToolController::setSelectedLayers(const QSet<int> &selectedLayers)
{
	m_selectedLayers = selectedLayers;
}

void ToolController::deselectDeletedAnnotation(int annotationId)
{
	if(m_activeAnnotation == annotationId) {
		setActiveAnnotation(0);
	}
}

void ToolController::flushPreviewedActions()
{
	m_activeTool->flushPreviewedActions();
}

Tool::Type ToolController::activeTool() const
{
	Q_ASSERT(m_activeTool);
	return m_activeTool->type();
}

QCursor ToolController::activeToolCursor() const
{
	Q_ASSERT(m_activeTool);
	return m_activeTool->cursor();
}

Capabilities ToolController::activeToolCapabilities() const
{
	Q_ASSERT(m_activeTool);
	return m_activeTool->capabilities();
}

void ToolController::setActiveLayer(int id)
{
	if(m_activeLayer != id) {
		if(m_selectionEditActive) {
			setSelectionEditActive(false);
		}
		m_activeLayer = id;
		if(m_model) {
			m_model->paintEngine()->setViewLayer(id);
		}
		if(m_activeTool) {
			m_activeTool->setActiveLayer(id);
		}
		updateTransformPreview();
		emit activeLayerChanged(id);
	}
}

void ToolController::setActiveBrush(const brushes::ActiveBrush &b)
{
	m_activebrush = b;
	emit activeBrushChanged(b);
}

void ToolController::setForegroundColor(const QColor &color)
{
	if(color != m_foregroundColor) {
		m_foregroundColor = color;
		if(m_activeTool) {
			m_activeTool->setForegroundColor(color);
		}
		emit foregroundColorChanged(color);
	}
}

void ToolController::setInterpolateInputs(bool interpolateInputs)
{
	m_interpolateInputs = interpolateInputs;
}

void ToolController::setStabilizationMode(
	brushes::StabilizationMode stabilizationMode)
{
	m_stabilizationMode = stabilizationMode;
	updateSmoothing();
}

void ToolController::setStabilizerSampleCount(int stabilizerSampleCount)
{
	m_stabilizerSampleCount = stabilizerSampleCount;
}

void ToolController::setSmoothing(int smoothing)
{
	m_smoothing = smoothing;
	updateSmoothing();
}

void ToolController::setFinishStrokes(bool finishStrokes)
{
	m_finishStrokes = finishStrokes;
}

void ToolController::setStabilizerUseBrushSampleCount(
	bool stabilizerUseBrushSampleCount)
{
	if(m_stabilizerUseBrushSampleCount != stabilizerUseBrushSampleCount) {
		m_stabilizerUseBrushSampleCount = stabilizerUseBrushSampleCount;
		emit stabilizerUseBrushSampleCountChanged(
			m_stabilizerUseBrushSampleCount);
	}
}

void ToolController::setModel(canvas::CanvasModel *model)
{
	m_model = model;
	connect(
		m_model->transform(), &canvas::TransformModel::transformChanged, this,
		&ToolController::updateTransformPreview);
	connect(
		m_model->transform(), &canvas::TransformModel::transformCut, this,
		&ToolController::setTransformCutPreview);
	connect(
		m_model->transform(), &canvas::TransformModel::transformCutCleared,
		this, &ToolController::clearTransformCutPreview);
	m_model->setTransformInterpolation(m_transformInterpolation);
	m_model->transform()->setPreviewAccurate(m_transformPreviewAccurate);
	emit modelChanged(model);
}

void ToolController::updateTransformPreview()
{
	if(m_model) {
		canvas::PaintEngine *paintEngine = m_model->paintEngine();
		canvas::TransformModel *transform = m_model->transform();
		if(transform->isActive() && transform->isDstQuadValid() &&
		   transform->isPreviewAccurate()) {
			QPoint point =
				transform->dstQuad().boundingRect().topLeft().toPoint();
			int blendMode = transform->blendMode();
			qreal opacity = transform->opacity();
			int x = point.x();
			int y = point.y();
			QPolygon dstPolygon = transform->dstQuad().polygon().toPolygon();
			int interpolation =
				transform->getEffectiveInterpolation(m_transformInterpolation);
			int idsUsed = 0;
			if(transform->isMovedFromCanvas()) {
				int singleLayerMoveId =
					transform->getSingleLayerMoveId(m_activeLayer);
				if(singleLayerMoveId > 0) {
					QImage layerImage =
						transform->layerImage(singleLayerMoveId);
					if(!layerImage.isNull()) {
						paintEngine->previewTransform(
							idsUsed++, m_activeLayer, blendMode, opacity, x, y,
							layerImage, dstPolygon, interpolation);
					}
				} else {
					for(int layerId : transform->layerIds()) {
						QImage layerImage = transform->layerImage(layerId);
						if(!layerImage.isNull()) {
							paintEngine->previewTransform(
								idsUsed++, layerId, blendMode, opacity, x, y,
								layerImage, dstPolygon, interpolation);
						}
					}
				}
			} else {
				paintEngine->previewTransform(
					idsUsed++, m_activeLayer, blendMode, opacity, x, y,
					transform->floatingImage(), dstPolygon, interpolation);
			}

			for(int i = idsUsed; i < m_transformPreviewIdsUsed; ++i) {
				paintEngine->clearTransformPreview(i);
			}
			m_transformPreviewIdsUsed = idsUsed;
		} else {
			paintEngine->clearAllTransformPreviews();
		}
	}
}

void ToolController::setTransformCutPreview(
	const QSet<int> &layerIds, const QRect &maskBounds, const QImage &mask)
{
	if(m_model) {
		canvas::PaintEngine *paintEngine = m_model->paintEngine();
		if(layerIds.isEmpty()) {
			paintEngine->clearCutPreview();
		} else {
			paintEngine->previewCut(layerIds, maskBounds, mask);
		}
	}
}

void ToolController::clearTransformCutPreview()
{
	if(m_model) {
		canvas::PaintEngine *paintEngine = m_model->paintEngine();
		paintEngine->clearCutPreview();
	}
}

void ToolController::clearTransformPreviews()
{
	if(m_model) {
		canvas::PaintEngine *paintEngine = m_model->paintEngine();
		paintEngine->clearAllTransformPreviews();
		paintEngine->clearCutPreview();
	}
}

void ToolController::setGlobalSmoothing(int smoothing)
{
	if(m_globalSmoothing != smoothing) {
		m_globalSmoothing = smoothing;
		updateSmoothing();
		emit globalSmoothingChanged(smoothing);
	}
}

void ToolController::setMouseSmoothing(bool mouseSmoothing)
{
	m_mouseSmoothing = mouseSmoothing;
}

void ToolController::updateSmoothing()
{
	int strength = m_globalSmoothing;
	if(m_stabilizationMode == brushes::Smoothing) {
		strength += m_smoothing;
	}
	m_effectiveSmoothing =
		qBound(0, strength, libclient::settings::maxSmoothing);
}

void ToolController::setTransformParams(bool accurate, int interpolation)
{
	if(accurate != m_transformPreviewAccurate ||
	   interpolation != m_transformInterpolation) {
		m_transformPreviewAccurate = accurate;
		m_transformInterpolation = interpolation;
		if(m_model) {
			m_model->transform()->setPreviewAccurate(accurate);
		}
		updateTransformPreview();
	}
}

void ToolController::setBrushSizeLimit(int brushSizeLimit)
{
	for(Tool *tool : m_toolbox) {
		tool->setBrushSizeLimit(brushSizeLimit);
	}
}

void ToolController::startDrawing(
	long long timeMsec, const QPointF &point, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation, bool right, qreal angle, qreal zoom,
	bool mirror, bool flip, bool constrain, bool center, const QPointF &viewPos,
	int deviceType, bool eraserOverride)
{
	Q_ASSERT(m_activeTool);
	if(m_model) {
		m_drawing = true;
		m_applyGlobalSmoothing =
			deviceType != int(DeviceType::Mouse) || m_mouseSmoothing;
		m_activebrush.setEraserOverride(eraserOverride);
		m_activeTool->begin(Tool::BeginParams{
			canvas::Point(timeMsec, point, pressure, xtilt, ytilt, rotation),
			viewPos, angle, zoom, DeviceType(deviceType), mirror, flip, right,
			constrain, center});

		if(!m_activeTool->isMultipart()) {
			m_model->paintEngine()->setLocalDrawingInProgress(true);
		}

		if(m_activeTool->usesBrushColor() && !m_activebrush.isEraser() &&
		   !m_activebrush.isEraserOverride()) {
			emit colorUsed(m_activebrush.qColor());
		}
	}
}

void ToolController::continueDrawing(
	long long timeMsec, const QPointF &point, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation, bool constrain, bool center,
	const QPointF &viewPos)
{
	Q_ASSERT(m_activeTool);
	if(m_model && m_drawing) {
		m_activeTool->motion(Tool::MotionParams{
			canvas::Point(timeMsec, point, pressure, xtilt, ytilt, rotation),
			viewPos, constrain, center});
	}
}

void ToolController::modifyDrawing(bool constrain, bool center)
{
	Q_ASSERT(m_activeTool);
	if(m_model) {
		m_activeTool->modify(Tool::ModifyParams{constrain, center});
	}
}

void ToolController::hoverDrawing(
	const QPointF &point, qreal angle, qreal zoom, bool mirror, bool flip,
	bool constrain, bool center)
{
	Q_ASSERT(m_activeTool);
	if(m_model) {
		m_activeTool->hover(Tool::HoverParams{
			point, angle, zoom, mirror, flip, constrain, center});
	}
}

void ToolController::endDrawing(bool constrain, bool center)
{
	Q_ASSERT(m_activeTool);
	if(m_model) {
		if(m_drawing) {
			m_drawing = false;
			m_activeTool->end(Tool::EndParams{constrain, center});
		}
		m_model->paintEngine()->setLocalDrawingInProgress(false);
	}
}

bool ToolController::undoMultipartDrawing()
{
	Q_ASSERT(!m_model || m_activeTool);
	if(m_model && m_activeTool->isMultipart()) {
		m_activeTool->undoMultipart();
		return true;
	} else {
		return false;
	}
}

bool ToolController::redoMultipartDrawing()
{
	Q_ASSERT(!m_model || m_activeTool);
	if(m_model && m_activeTool->isMultipart()) {
		m_activeTool->redoMultipart();
		return true;
	} else {
		return false;
	}
}

bool ToolController::isMultipartDrawing() const
{
	Q_ASSERT(m_activeTool);

	return m_activeTool->isMultipart();
}

void ToolController::finishMultipartDrawing()
{
	Q_ASSERT(m_activeTool);

	if(!m_model) {
		qWarning("ToolController::finishMultipartDrawing: no model set!");
		return;
	}

	m_activeTool->finishMultipart();
}

void ToolController::cancelMultipartDrawing()
{
	Q_ASSERT(m_activeTool);

	if(!m_model) {
		qWarning("ToolController::cancelMultipartDrawing: no model set!");
		return;
	}

	m_activeTool->cancelMultipart();
	emit actionCancelled();
}

void ToolController::offsetActiveTool(int xOffset, int yOffset)
{
	Q_ASSERT(m_activeTool);
	m_activeTool->offsetActiveTool(xOffset, yOffset);
}

void ToolController::setBrushEngineBrush(
	drawdance::BrushEngine &be, bool freehand)
{
	const brushes::ActiveBrush &brush = activeBrush();
	DP_StrokeParams stroke = {
		activeLayerOrSelection(),
		false,
		0,
		m_stabilizationMode != brushes::Smoothing || m_finishStrokes,
		0,
		m_finishStrokes,
	};
	if(freehand) {
		stroke.interpolate = m_interpolateInputs;
		stroke.smoothing =
			m_applyGlobalSmoothing || m_stabilizationMode == brushes::Smoothing
				? m_effectiveSmoothing
				: m_smoothing;
		if(m_stabilizerUseBrushSampleCount) {
			if(brush.stabilizationMode() == brushes::Stabilizer) {
				stroke.stabilizer_sample_count = brush.stabilizerSampleCount();
			}
		} else if(m_stabilizationMode == brushes::Stabilizer) {
			stroke.stabilizer_sample_count = m_stabilizerSampleCount;
		}
	}
	brush.setInBrushEngine(be, stroke);
}

void ToolController::executeAsync(Task *task)
{
	task->setParent(this);
	if(m_taskCount++ == 0) {
		emit toolStateChanged(int(ToolState::Busy));
	}
	m_threadPool.start(utils::FunctionRunnable::create([=]() {
		task->run();
		emit asyncExecutionFinished(task);
		if(--m_taskCount == 0) {
			emit toolStateChanged(int(ToolState::Normal));
		}
	}));
}

void ToolController::notifyAsyncExecutionFinished(Task *task)
{
	task->finished();
	task->deleteLater();
}

void ToolController::requestSelect(const Tool::BeginParams &params, int type)
{
	m_hotSwapParams = params;
	emit selectRequested(type);
}

void ToolController::beginRectangleSelection()
{
	RectangleSelection *rs =
		static_cast<RectangleSelection *>(m_toolbox[Tool::SELECTION]);
	if(m_activeTool == rs) {
		rs->setForceSelect(true);
		startDrawingFromHotSwapParams();
		rs->setForceSelect(false);
	} else {
		qWarning(
			"beginRectangleSelection: rectangle selection tool is not active");
	}
}

void ToolController::beginPolygonSelection()
{
	PolygonSelection *ps =
		static_cast<PolygonSelection *>(m_toolbox[Tool::POLYGONSELECTION]);
	if(m_activeTool == ps) {
		ps->setForceSelect(true);
		startDrawingFromHotSwapParams();
		ps->setForceSelect(false);
	} else {
		qWarning("beginPolygonSelection: polygon selection tool is not active");
	}
}

void ToolController::requestTransformMove(
	const Tool::BeginParams &params, bool maskOnly, bool quickMove)
{
	m_hotSwapParams = params;
	emit transformRequested(maskOnly, true, quickMove);
}

void ToolController::beginTransformMove(bool applyOnEnd)
{
	TransformTool *tt = transformTool();
	if(m_activeTool == tt) {
		tt->setForceMove(true);
		tt->setApplyOnEnd(applyOnEnd);
		startDrawingFromHotSwapParams();
		tt->setForceMove(false);
	} else {
		qWarning("beginTransformMove: transform tool is not active");
	}
}

void ToolController::startDrawingFromHotSwapParams()
{
	const canvas::Point &point = m_hotSwapParams.point;
	startDrawing(
		point.timeMsec(), point, point.pressure(), point.xtilt(), point.ytilt(),
		point.rotation(), m_hotSwapParams.right, m_hotSwapParams.angle,
		m_hotSwapParams.zoom, m_hotSwapParams.mirror, m_hotSwapParams.flip,
		m_hotSwapParams.constrain, m_hotSwapParams.center,
		m_hotSwapParams.viewPos, int(m_hotSwapParams.deviceType), false);
}

}
