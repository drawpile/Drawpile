// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/liquify.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/selectionmodel.h"
#include "libclient/canvas/transformmodel.h"
#include "libclient/drawdance/global.h"
#include "libclient/image/geom.h"
#include "libclient/net/client.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/utils/cursors.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QTransform>

namespace tools {

LiquifyTool::LiquifyTool(ToolController &owner)
	: Tool(
		  owner, LIQUIFY, Qt::PointingHandCursor,
		  Capability::Fractional | Capability::SupportsPressure)
{
}

void LiquifyTool::begin(const BeginParams &params)
{
	if(isLiquifyActive()) {
		m_operation = m_props.operation;
		m_reverse = m_props.reverse;
		m_owner.setLiquifyEngineParams(
			m_liquifyEngine, m_props.size, m_props.amount, m_props.hardness,
			m_props.spacing, m_props.sizePressure, m_props.amountPressure,
			m_props.hardnessPressure);
		m_firstPoint = params.point;
		m_zoom = params.zoom;
		m_drawing = true;
		m_strokeStarted = false;

	} else {
		m_drawing = false;
		tryBeginLiquify();
	}
}

void LiquifyTool::motion(const MotionParams &params)
{
	if(m_drawing) {
		canvas::Point point = params.point;
		if(m_strokeStarted) {
			m_liquifyEngine.strokeTo(point);
			processDabs();
		} else {
			if(geom::pointFuzzyEqual(m_firstPoint, point, 1e-6)) {
				m_firstPoint.setPressure(point.pressure());
			} else {
				m_strokeStarted = true;
				m_liquifyEngine.beginStroke(m_zoom);
				m_liquifyEngine.strokeTo(point);
				processDabs();
			}
		}
	}
}

void LiquifyTool::end(const EndParams &params)
{
	Q_UNUSED(params);
	if(m_drawing) {
		m_drawing = false;
		if(m_strokeStarted) {
			m_strokeStarted = false;
			m_liquifyEngine.endStroke(QDateTime::currentMSecsSinceEpoch());
			processDabs();
			canvas::TransformModel *transform = getActiveLiquifyModel();
			if(transform) {
				pushState(transform);
			}
		}
	}
}

void LiquifyTool::finishMultipart()
{
	canvas::TransformModel *transform = getActiveLiquifyModel();
	bool allowed = true;
	if(transform && (allowed = transform->isAllowedToApplyActiveTransform())) {
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
		endLiquify(transform, false);
	}
	returnToPreviousTool();
}

void LiquifyTool::undoMultipart()
{
	canvas::TransformModel *transform = getActiveLiquifyModel();
	if(transform) {
		if(m_stateStackTop > 0) {
			--m_stateStackTop;
			transform->setLiquifyState(m_stateStack[m_stateStackTop]);
		} else {
			cancelMultipart();
		}
	}
}

void LiquifyTool::redoMultipart()
{
	canvas::TransformModel *transform = getActiveLiquifyModel();
	if(transform && m_stateStackTop + 1 < m_stateStack.size()) {
		++m_stateStackTop;
		transform->setLiquifyState(m_stateStack[m_stateStackTop]);
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

canvas::TransformModel *LiquifyTool::tryBeginLiquify()
{
	Q_ASSERT(!isLiquifyActive());
	Q_ASSERT(m_stateStack.isEmpty());

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
		m_owner.liquifyInterpolation());
	m_stateStack.clear();
	m_stateStack.append(transform->liquifyState());
	m_stateStackTop = 0;
	setCursor(utils::Cursors::liquify());
	return transform;
}

void LiquifyTool::endLiquify(canvas::TransformModel *transform, bool applied)
{
	transform->endActiveTransform(applied);
	m_stateStack.clear();
	m_stateStackTop = -1;
	setCursor(Qt::PointingHandCursor);
}

void LiquifyTool::processDabs()
{
	if(m_liquifyEngine.hasDabs()) {
		canvas::TransformModel *transform = getActiveLiquifyModel();
		if(transform) {
			transform->liquify(
				std::bind(
					&LiquifyTool::applyDabs, this, std::placeholders::_1));
		} else {
			qWarning("LiquifyTool::processDabs: liquify not active");
		}
		m_liquifyEngine.clearDabs();
	}
}

void LiquifyTool::applyDabs(drawdance::Liquify &liquify)
{
	drawdance::DrawContext drawContext = drawdance::DrawContextPool::acquire();
	for(const DP_LiquifyEngineDab &dab : m_liquifyEngine.dabs()) {
		qreal sigma = dab.size / 3.0;
		switch(m_operation) {
		case Operation::Move: {
			qreal amount = effectiveAmount(dab.amount);
			qreal offsetLength = amount * sigma;
			qreal offsetAngle = dab.direction_rad;
			QPointF offset =
				QPointF(qCos(offsetAngle), qSin(offsetAngle)) * offsetLength;
			liquify.opMove(
				drawContext.get(), dab.x, dab.y, dab.size, dab.hardness,
				offset.x(), offset.y());
			break;
		}
		case Operation::Scale: {
			qreal amount = effectiveAmount(dab.amount);
			liquify.opScale(
				drawContext.get(), dab.x, dab.y, dab.size, dab.hardness,
				amount);
			break;
		}
		case Operation::Rotate: {
			qreal amount = effectiveAmount(dab.amount);
			qreal angle = 2.0 * M_PI * amount;
			liquify.opRotate(
				drawContext.get(), dab.x, dab.y, dab.size, dab.hardness, angle);
			break;
		}
		case Operation::Smoothe:
			liquify.opSmoothe(
				drawContext.get(), dab.x, dab.y, dab.size, dab.hardness,
				qBound(0.0f, dab.amount, 1.0f), SMOOTHE_KERNEL_RADIUS);
			break;
		case Operation::Undo:
			liquify.opErase(
				drawContext.get(), dab.x, dab.y, dab.size, dab.hardness,
				qBound(0.0f, dab.amount, 1.0f));
			break;
		default:
			Q_UNREACHABLE();
		}
	}
}

void LiquifyTool::pushState(canvas::TransformModel *transform)
{
	int popCount = m_stateStack.size() - m_stateStackTop - 1;
	if(popCount > 0) {
		m_stateStack.remove(m_stateStackTop + 1, popCount);
	}

	int shiftCount = (m_stateStack.size() + 1) - MAX_STATE_STACK_DEPTH;
	if(shiftCount > 0) {
		m_stateStack.remove(0, shiftCount);
	}

	m_stateStack.append(transform->liquifyState());
	m_stateStackTop = m_stateStack.size() - 1;
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
