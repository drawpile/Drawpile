// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/tools/toolcontroller.h"

#include "libclient/settings.h"
#include "libclient/tools/annotation.h"
#include "libclient/tools/freehand.h"
#include "libclient/tools/colorpicker.h"
#include "libclient/tools/laser.h"
#include "libclient/tools/selection.h"
#include "libclient/tools/shapetools.h"
#include "libclient/tools/beziertool.h"
#include "libclient/tools/floodfill.h"
#include "libclient/tools/zoom.h"
#include "libclient/tools/inspector.h"

#include "libclient/canvas/point.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libshared/util/functionrunnable.h"

namespace tools {

ToolController::ToolController(net::Client *client, QObject *parent)
	: QObject(parent)
	, m_toolbox{}
	, m_client(client)
	, m_model(nullptr)
	, m_activeTool(nullptr)
	, m_globalSmoothing(0)
	, m_interpolateInputs(false)
	, m_stabilizationMode(brushes::Stabilizer)
	, m_stabilizerSampleCount(0)
	, m_smoothing(0)
	, m_effectiveSmoothing(0)
	, m_finishStrokes(true)
	, m_stabilizerUseBrushSampleCount(true)
	, m_selectInterpolation{DP_MSG_TRANSFORM_REGION_MODE_BILINEAR}
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
	registerTool(new ZoomTool(*this));
	registerTool(new Inspector(*this));

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
	// May be null if a file failed to open or a session didn't connect.
	if(m_model) {
		for(Tool *t : m_toolbox) {
			t->cancelMultipart(); // Also cancels any asynchronous jobs running.
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

void ToolController::setActiveTool(Tool::Type tool)
{
	if(activeTool() != tool) {
		m_activeTool->cancelMultipart();
		endDrawing();

		m_activeTool = getTool(tool);
		emit toolCapabilitiesChanged(
			activeToolAllowColorPick(), activeToolAllowToolAdjust(),
			activeToolHandlesRightClick());
		emit toolCursorChanged(activeToolCursor());
	}
}

void ToolController::setActiveAnnotation(uint16_t id)
{
	if(m_activeAnnotation != id) {
		m_activeAnnotation = id;
		emit activeAnnotationChanged(id);
	}
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

bool ToolController::activeToolAllowColorPick() const
{
	Q_ASSERT(m_activeTool);
	return m_activeTool->allowColorPick();
}

bool ToolController::activeToolAllowToolAdjust() const
{
	Q_ASSERT(m_activeTool);
	return m_activeTool->allowToolAdjust();
}

bool ToolController::activeToolHandlesRightClick() const
{
	Q_ASSERT(m_activeTool);
	return m_activeTool->handlesRightClick();
}

void ToolController::setActiveLayer(uint16_t id)
{
	if(m_activeLayer != id) {
		m_activeLayer = id;
		if(m_model) {
			m_model->paintEngine()->setViewLayer(id);
		}

		updateSelectionPreview();
		emit activeLayerChanged(id);
	}
}

void ToolController::setActiveBrush(const brushes::ActiveBrush &b)
{
	m_activebrush = b;
	emit activeBrushChanged(b);
}

void ToolController::setInterpolateInputs(bool interpolateInputs)
{
	m_interpolateInputs = interpolateInputs;
}

void ToolController::setStabilizationMode(brushes::StabilizationMode stabilizationMode)
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

void ToolController::setStabilizerUseBrushSampleCount(bool stabilizerUseBrushSampleCount)
{
	if(m_stabilizerUseBrushSampleCount != stabilizerUseBrushSampleCount) {
		m_stabilizerUseBrushSampleCount = stabilizerUseBrushSampleCount;
		emit stabilizerUseBrushSampleCountChanged(m_stabilizerUseBrushSampleCount);
	}
}

void ToolController::setModel(canvas::CanvasModel *model)
{
	m_model = model;
	connect(
		m_model->aclState(), &canvas::AclState::featureAccessChanged, this,
		&ToolController::onFeatureAccessChange);
	connect(
		m_model, &canvas::CanvasModel::selectionChanged, this,
		&ToolController::onSelectionChange);
	emit modelChanged(model);
}

void ToolController::onFeatureAccessChange(DP_Feature feature, bool canUse)
{
	if(feature == DP_FEATURE_REGION_MOVE) {
		static_cast<SelectionTool *>(getTool(Tool::SELECTION))
			->setTransformEnabled(canUse);
		static_cast<SelectionTool *>(getTool(Tool::POLYGONSELECTION))
			->setTransformEnabled(canUse);
	}
}

void ToolController::onSelectionChange(canvas::Selection *sel)
{
	if(sel) {
		connect(
			sel, &canvas::Selection::pasteImageChanged, this,
			&ToolController::updateSelectionPreview);
		connect(
			sel, &canvas::Selection::shapeChanged, this,
			&ToolController::updateSelectionPreview);
	}
	updateSelectionPreview();
}

void ToolController::updateSelectionPreview()
{
	if(!m_model) {
		return;
	}

	canvas::PaintEngine *paintEngine = m_model->paintEngine();
	canvas::Selection *sel = m_model->selection();
	if(sel && sel->hasPasteImage()) {
		QPoint point = sel->shape().boundingRect().topLeft().toPoint();
		paintEngine->previewTransform(
			m_activeLayer, point.x(), point.y(), sel->pasteImage(),
			sel->destinationQuad(), m_selectInterpolation);
	} else {
		paintEngine->clearTransformPreview();
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

void ToolController::updateSmoothing()
{
	int strength = m_globalSmoothing;
	if(m_stabilizationMode == brushes::Smoothing) {
		strength += m_smoothing;
	}
	m_effectiveSmoothing =
		qBound(0, strength, libclient::settings::maxSmoothing);
}

void ToolController::setSelectInterpolation(int selectInterpolation)
{
	m_selectInterpolation = selectInterpolation;
	updateSelectionPreview();
}

void ToolController::startDrawing(
	long long timeMsec, const QPointF &point, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation, bool right, float zoom)
{
	Q_ASSERT(m_activeTool);

	if(!m_model) {
		qWarning("ToolController::startDrawing: no model set!");
		return;
	}

	m_activeTool->begin(canvas::Point(timeMsec, point, pressure, xtilt, ytilt, rotation), right, zoom);

	if(!m_activeTool->isMultipart()) {
		m_model->paintEngine()->setLocalDrawingInProgress(true);
	}

	if(m_activeTool->usesBrushColor() && !m_activebrush.isEraser())
		emit colorUsed(m_activebrush.qColor());
}

void ToolController::continueDrawing(
	long long timeMsec, const QPointF &point, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation, bool shift, bool alt)
{
	Q_ASSERT(m_activeTool);

	if(!m_model) {
		qWarning("ToolController::continueDrawing: no model set!");
		return;
	}

	canvas::Point cp = canvas::Point(timeMsec, point, pressure, xtilt, ytilt, rotation);
	m_activeTool->motion(cp, shift, alt);
}

void ToolController::hoverDrawing(const QPointF &point)
{
	Q_ASSERT(m_activeTool);
	if(!m_model)
		return;

	m_activeTool->hover(point);
}

void ToolController::endDrawing()
{
	Q_ASSERT(m_activeTool);

	if(!m_model) {
		qWarning("ToolController::endDrawing: no model set!");
		return;
	}

	m_activeTool->end();
	m_model->paintEngine()->setLocalDrawingInProgress(false);
}

bool ToolController::undoMultipartDrawing()
{
	Q_ASSERT(m_activeTool);

	if(!m_model) {
		qWarning("ToolController::undoMultipartDrawing: no model set!");
		return false;
	}

	if(!m_activeTool->isMultipart())
		return false;

	m_activeTool->undoMultipart();
	return true;
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

	if(m_model->aclState()->isLayerLocked(m_activeLayer)) {
		// It is possible for the active layer to become locked
		// before the user has finished multipart drawing.
		qWarning("Cannot finish multipart drawing: active layer is locked!");
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
		activeLayer(),
		false,
		0,
		m_stabilizationMode != brushes::Smoothing || m_finishStrokes,
		0,
		m_finishStrokes,
	};
	if(freehand) {
		stroke.interpolate = m_interpolateInputs;
		stroke.smoothing = m_effectiveSmoothing;
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
		emit busyStateChanged(true);
	}
	m_threadPool.start(utils::FunctionRunnable::create([=](){
		task->run();
		emit asyncExecutionFinished(task);
		if(--m_taskCount == 0) {
			emit busyStateChanged(false);
		}
	}));
}

void ToolController::notifyAsyncExecutionFinished(Task *task)
{
	task->finished();
	task->deleteLater();
}

}
