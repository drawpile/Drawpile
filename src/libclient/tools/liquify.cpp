// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/liquify.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/selectionmodel.h"
#include "libclient/canvas/transformmodel.h"
#include "libclient/net/client.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/utils/cursors.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QTransform>
#include <QtMath>

namespace tools {

LiquifyTool::LiquifyTool(ToolController &owner)
	: Tool(
		  owner, LIQUIFY, Qt::PointingHandCursor,
		  Capability::Fractional | Capability::SupportsPressure |
			  Capability::AllowToolAdjust1 | Capability::AllowToolAdjust2 |
			  Capability::AllowToolAdjust3)
{
}

void LiquifyTool::begin(const BeginParams &params)
{
	drawdance::LiquifyWorker *liquifyWorker = getActiveLiquifyWorker();
	if(liquifyWorker) {
		// It can be pretty hard to tell if e.g. the smoothe action is actually
		// really busy smoothing out stuff while doing absolutely nothing, so
		// the user may start strokeing around and get confused why nothing is
		// happening. We'll just cancel t he running stroke and assume the user
		// doesn't care about whatever trailing stuff it's still doing to work
		// against that. Also, right-click just cancels.
		liquifyWorker->cancelStroke(QDateTime::currentMSecsSinceEpoch());
		if(!params.right) {
			setLiquifyWorkerOperation(liquifyWorker);
			m_owner.setLiquifyWorkerStrokeParams(
				*liquifyWorker, m_props.size, m_props.amount, m_props.hardness,
				m_props.spacing, m_props.sizePressure, m_props.amountPressure,
				m_props.hardnessPressure);
			m_firstPoint = params.point;
			m_zoom = params.zoom;
			m_drawing = true;

			// If there is no pressure-related inputs or the user explicitly
			// requested there be no initial dab delay, we start a stroke
			// immediately. Otherwise we wait for it to stabilize.
			bool havePressureDynamics = m_props.sizePressure ||
										m_props.amountPressure ||
										m_props.hardnessPressure;
			if(havePressureDynamics && m_owner.delayInitialDab()) {
				m_strokeStarted = false;
			} else {
				m_strokeStarted = true;
				liquifyWorker->beginStroke(m_zoom);
				liquifyWorker->strokeTo(
					m_firstPoint, params.constrain, params.center);
			}
		}
	} else if(!params.right) {
		m_drawing = false;
		tryBeginLiquify();
	}
}

void LiquifyTool::motion(const MotionParams &params)
{
	if(m_drawing) {
		canvas::Point point = params.point;
		if(m_strokeStarted) {
			drawdance::LiquifyWorker *liquifyWorker = getActiveLiquifyWorker();
			if(liquifyWorker) {
				liquifyWorker->strokeTo(point, params.constrain, params.center);
			}
		} else {
			if(qAbs(m_firstPoint.x() - point.x()) < 1e-6 ||
			   qAbs(m_firstPoint.y() - point.y()) < 1e-6) {
				m_firstPoint.setPressure(point.pressure());
			} else {
				m_strokeStarted = true;
				drawdance::LiquifyWorker *liquifyWorker =
					getActiveLiquifyWorker();
				if(liquifyWorker) {
					liquifyWorker->beginStroke(m_zoom);
					liquifyWorker->strokeTo(
						m_firstPoint, params.constrain, params.center);
					liquifyWorker->strokeTo(
						point, params.constrain, params.center);
				}
			}
		}
	}
}

void LiquifyTool::end(const EndParams &params)
{
	Q_UNUSED(params);
	if(m_drawing) {
		m_drawing = false;
		drawdance::LiquifyWorker *liquifyWorker = getActiveLiquifyWorker();
		if(liquifyWorker) {
			if(!m_strokeStarted) {
				liquifyWorker->beginStroke(m_zoom);
				liquifyWorker->strokeTo(
					m_firstPoint, params.constrain, params.center);
			}
			liquifyWorker->endStroke(QDateTime::currentMSecsSinceEpoch());
		}
		m_strokeStarted = false;
	}
}

void LiquifyTool::finishMultipart()
{
	canvas::TransformModel *transform = getActiveLiquifyModel();
	bool allowed = true;
	if(transform && (allowed = transform->isAllowedToApplyActiveTransform())) {
		drawdance::LiquifyWorker &liquifyWorker = transform->liquifyWorker();
		liquifyWorker.abort();
		liquifyWorker.finishThread();
		net::Client *client = m_owner.client();
		net::MessageList msgs = transform->applyActiveTransform(
			client->myId(), m_owner.activeLayer(),
			m_owner.liquifyInterpolation(), false);
		int count = msgs.size();
		if(count == 0) {
			endLiquify(transform, false);
		} else {
			client->sendCommands(count, msgs.constData());
			endLiquify(transform, true);
		}
	}

	if(allowed) {
		returnToPreviousTool();
	} else {
		emit m_owner.showMessageRequested(
			QCoreApplication::translate(
				"tools::TransformSettings",
				"You don't have permission to cut and paste."));
	}
}

void LiquifyTool::cancelMultipart()
{
	canvas::TransformModel *transform = getActiveLiquifyModel();
	if(transform) {
		transform->liquifyWorker().abort();
		endLiquify(transform, false);
	}
	returnToPreviousTool();
}

void LiquifyTool::undoMultipart()
{
	drawdance::LiquifyWorker *liquifyWorker = getActiveLiquifyWorker();
	if(liquifyWorker) {
		liquifyWorker->cancelStroke(QDateTime::currentMSecsSinceEpoch());
		liquifyWorker->undo();
	}
}

void LiquifyTool::redoMultipart()
{
	drawdance::LiquifyWorker *liquifyWorker = getActiveLiquifyWorker();
	if(liquifyWorker) {
		liquifyWorker->cancelStroke(QDateTime::currentMSecsSinceEpoch());
		liquifyWorker->redo();
	}
}

bool LiquifyTool::isMultipart() const
{
	return isLiquifyActive();
}

void LiquifyTool::beginTemporary(Tool::Type toolToReturnTo)
{
	if(!isLiquifyActive()) {
		m_toolToReturnTo = toolToReturnTo;
		tryBeginLiquify();
	}
}

void LiquifyTool::clearTemporary()
{
	m_toolToReturnTo = Tool::Type::_LASTTOOL;
}

bool LiquifyTool::handleDeselect(bool finish)
{
	canvas::TransformModel *transform = getActiveLiquifyModel();
	if(transform) {
		transform->setDeselectOnApply(true);
		if(finish) {
			finishMultipart();
			return true;
		}
	}
	return false;
}

bool LiquifyTool::isLiquifyActive() const
{
	canvas::CanvasModel *canvas = m_owner.model();
	return canvas && canvas->transform()->isLiquifyActive();
}

canvas::TransformModel *LiquifyTool::getActiveLiquifyModel() const
{
	canvas::CanvasModel *canvas = m_owner.model();
	if(canvas) {
		canvas::TransformModel *transform = canvas->transform();
		if(transform->isLiquifyActive()) {
			return transform;
		}
	}
	return nullptr;
}

drawdance::LiquifyWorker *LiquifyTool::getActiveLiquifyWorker() const
{
	canvas::CanvasModel *canvas = m_owner.model();
	if(canvas) {
		drawdance::LiquifyWorker &liquifyWorker =
			canvas->transform()->liquifyWorker();
		if(!liquifyWorker.isNull()) {
			return &liquifyWorker;
		}
	}
	return nullptr;
}

canvas::TransformModel *LiquifyTool::tryBeginLiquify()
{
	Q_ASSERT(!isLiquifyActive());

	canvas::CanvasModel *canvas = m_owner.model();
	if(!canvas) {
		emit m_owner.showMessageRequested(
			QCoreApplication::translate(
				"tools::TransformSettings", "No canvas present."));
		returnToPreviousTool();
		return nullptr;
	}

	if(!canvas->aclState()->canUseFeature(DP_FEATURE_PUT_IMAGE)) {
		emit m_owner.showMessageRequested(
			QCoreApplication::translate(
				"tools::TransformSettings",
				"You don't have permission to cut and paste."));
		returnToPreviousTool();
		return nullptr;
	}

	canvas::SelectionModel *selection = canvas->selection();
	if(!selection->isValid()) {
		emit m_owner.showMessageRequested(
			QCoreApplication::translate(
				"tools::TransformSettings",
				"Nothing selected that could be liquified."));
		returnToPreviousTool();
		return nullptr;
	}

	canvas::TransformModel *transform = canvas->transform();
	transform->beginLiquifyFromCanvas(
		selection->bounds(), selection->image(), m_owner.selectedLayers(),
		m_owner.liquifyInterpolation(), UNDO_DEPTH);
	setCursor(utils::Cursors::liquify());
	return transform;
}

void LiquifyTool::endLiquify(canvas::TransformModel *transform, bool applied)
{
	transform->endActiveTransform(applied);
	setCursor(Qt::PointingHandCursor);
}

void LiquifyTool::setLiquifyWorkerOperation(
	drawdance::LiquifyWorker *liquifyWorker)
{
	switch(m_props.operation) {
	case Operation::Move:
		liquifyWorker->setOperationMove(m_props.reverse);
		return;
	case Operation::Scale:
		liquifyWorker->setOperationScale(m_props.reverse);
		return;
	case Operation::Rotate:
		liquifyWorker->setOperationRotate(m_props.reverse);
		return;
	case Operation::Smoothe:
		liquifyWorker->setOperationSmoothe(SMOOTHE_KERNEL_RADIUS);
		return;
	case Operation::Erase:
		liquifyWorker->setOperationErase();
		return;
	}
	Q_UNREACHABLE();
}

void LiquifyTool::returnToPreviousTool()
{
	if(m_toolToReturnTo != Tool::Type::_LASTTOOL) {
		Tool::Type toolToReturnTo = m_toolToReturnTo;
		m_toolToReturnTo = Tool::Type::_LASTTOOL;
		emit m_owner.toolSwitchRequested(toolToReturnTo);
	}
}

}
