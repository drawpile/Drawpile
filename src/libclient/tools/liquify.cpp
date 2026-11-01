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
		cancelStroke(liquifyWorker);
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
					m_firstPoint.posAdjustedBy(getLiquifyOffset()),
					params.constrain, params.center);
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
				liquifyWorker->strokeTo(
					point.posAdjustedBy(getLiquifyOffset()), params.constrain,
					params.center);
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
					QPointF liquifyOffset = getLiquifyOffset();
					liquifyWorker->beginStroke(m_zoom);
					liquifyWorker->strokeTo(
						m_firstPoint.posAdjustedBy(liquifyOffset),
						params.constrain, params.center);
					liquifyWorker->strokeTo(
						point.posAdjustedBy(liquifyOffset), params.constrain,
						params.center);
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
					m_firstPoint.posAdjustedBy(getLiquifyOffset()),
					params.constrain, params.center);
			}
			liquifyWorker->endStroke(QDateTime::currentMSecsSinceEpoch());
		}
		m_strokeStarted = false;
	}
}

bool LiquifyTool::copyCut(bool cut)
{
	drawdance::LiquifyWorker *liquifyWorker = getActiveLiquifyWorker();
	if(liquifyWorker) {
		cancelStroke(liquifyWorker);
		if(cut) {
			liquifyWorker->cut();
		} else {
			liquifyWorker->copy();
		}
		return true;
	} else {
		return false;
	}
}

void LiquifyTool::applyCopyCut(bool cut)
{
	canvas::TransformModel *transform = getActiveLiquifyModel();
	if(transform) {
		transform->copyLiquify();
		if(cut) {
			net::Client *client = m_owner.client();
			net::MessageList msgs = transform->cutSource(client->myId());
			int count = msgs.size();
			if(count != 0) {
				client->sendCommands(count, msgs.constData());
			}
			endLiquify(transform, false);
		}
	}
}

void LiquifyTool::finishMultipart()
{
	apply(false);
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
		cancelStroke(liquifyWorker);
		liquifyWorker->undo();
	}
}

void LiquifyTool::redoMultipart()
{
	drawdance::LiquifyWorker *liquifyWorker = getActiveLiquifyWorker();
	if(liquifyWorker) {
		cancelStroke(liquifyWorker);
		liquifyWorker->redo();
	}
}

bool LiquifyTool::isMultipart() const
{
	return isLiquifyActive();
}

void LiquifyTool::offsetActiveTool(int x, int y)
{
	canvas::TransformModel *transform = getActiveLiquifyModel();
	if(transform) {
		transform->applyOffset(x, y);
	}
}

void LiquifyTool::beginTemporary(Tool::Type toolToReturnTo)
{
	if(!isLiquifyActive()) {
		m_toolToReturnTo = toolToReturnTo;
		tryBeginLiquify();
	}
}

void LiquifyTool::beginTemporaryPaste(
	Tool::Type toolToReturnTo, const QImage &img, const QByteArray &liquifyData)
{
	if(!isLiquifyActive()) {
		m_toolToReturnTo = toolToReturnTo;
		tryBeginLiquify(&img, &liquifyData);
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

void LiquifyTool::stamp()
{
	drawdance::LiquifyWorker *liquifyWorker = getActiveLiquifyWorker();
	if(liquifyWorker) {
		cancelStroke(liquifyWorker);
		liquifyWorker->stamp();
	}
}

void LiquifyTool::applyStamp()
{
	apply(true);
}

void LiquifyTool::updateDisplacement(float displacement)
{
	drawdance::LiquifyWorker *liquifyWorker = getActiveLiquifyWorker();
	if(liquifyWorker) {
		liquifyWorker->setDisplacement(displacement);
	}
}

void LiquifyTool::applyDisplacement()
{
	drawdance::LiquifyWorker *liquifyWorker = getActiveLiquifyWorker();
	if(liquifyWorker) {
		liquifyWorker->applyDisplacement();
	}
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

QPointF LiquifyTool::getLiquifyOffset() const
{
	return -QPointF(m_owner.model()->transform()->liquifyCanvasOffset());
}

canvas::TransformModel *
LiquifyTool::tryBeginLiquify(const QImage *img, const QByteArray *liquifyData)
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

	if(img && liquifyData) {
		drawdance::Liquify liquify = drawdance::Liquify::deserialize(
			*liquifyData, img->width(), img->height());
		if(liquify.isNull()) {
			returnToPreviousTool();
			return nullptr;
		}

		canvas::TransformModel *transform = canvas->transform();
		transform->beginLiquifyPaste(
			liquify, *img, m_owner.liquifyInterpolation(), UNDO_DEPTH);
		setCursor(utils::Cursors::liquify());
		return transform;

	} else {
		canvas::SelectionModel *selection = canvas->selection();
		if(!selection->isValid()) {
			Q_EMIT m_owner.showMessageRequested(
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

bool LiquifyTool::apply(bool stamp)
{
	canvas::TransformModel *transform = getActiveLiquifyModel();
	bool allowed = true;
	bool applied = false;
	if(transform && (allowed = transform->isAllowedToApplyActiveTransform())) {
		if(!stamp) {
			drawdance::LiquifyWorker &liquifyWorker =
				transform->liquifyWorker();
			liquifyWorker.abort();
			liquifyWorker.finishThread();
		}

		net::Client *client = m_owner.client();
		net::MessageList msgs = transform->applyActiveTransform(
			client->myId(), m_owner.activeLayer(),
			m_owner.liquifyInterpolation(), stamp);

		int count = msgs.size();
		bool haveMessages = count != 0;
		if(haveMessages) {
			client->sendCommands(count, msgs.constData());
			applied = true;
		}

		if(!stamp) {
			endLiquify(transform, haveMessages);
		}
	}

	if(allowed) {
		if(!stamp) {
			returnToPreviousTool();
		}
	} else {
		Q_EMIT m_owner.showMessageRequested(
			QCoreApplication::translate(
				"tools::TransformSettings",
				"You don't have permission to cut and paste."));
	}

	return applied;
}

void LiquifyTool::returnToPreviousTool()
{
	if(m_toolToReturnTo != Tool::Type::_LASTTOOL) {
		Tool::Type toolToReturnTo = m_toolToReturnTo;
		m_toolToReturnTo = Tool::Type::_LASTTOOL;
		emit m_owner.toolSwitchRequested(toolToReturnTo);
	}
}

void LiquifyTool::cancelStroke(drawdance::LiquifyWorker *liquifyWorker)
{
	liquifyWorker->cancelStroke(QDateTime::currentMSecsSinceEpoch());
}

}
