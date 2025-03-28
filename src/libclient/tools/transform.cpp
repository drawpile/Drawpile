// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/transform.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/selectionmodel.h"
#include "libclient/canvas/transformmodel.h"
#include "libclient/net/client.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/utils/cursors.h"
#include <QCoreApplication>
#include <QPair>
#include <cmath>

using utils::Cursors;

namespace tools {

TransformTool::TransformTool(ToolController &owner)
	: Tool(
		  owner, TRANSFORM, QCursor(Qt::PointingHandCursor),
		  Capability::Fractional)
{
}

void TransformTool::begin(const BeginParams &params)
{
	m_clickDetector.begin(params.viewPos, params.deviceType);
	m_angle = params.angle;
	m_zoom = params.zoom;
	m_swapDiagonal = params.mirror != params.flip;
	m_invertMode = params.right;
	canvas::TransformModel *transform = getActiveTransformModel();
	if(!transform) {
		transform = tryBeginMove(true, false, Mode::Scale);
	}
	updateHoverHandle(transform, params.point);
	if(transform) {
		if(!m_forceMove && m_mode == Mode::Move &&
		   m_hoverHandle == Handle::Outside) {
			if(m_toolToReturnTo == Tool::SELECTION ||
			   m_toolToReturnTo == Tool::POLYGONSELECTION) {
				int type = int(m_toolToReturnTo);
				m_toolToReturnTo = Tool::_LASTTOOL;
				finishMultipart();
				m_owner.requestSelect(params, type);
			} else {
				finishMultipart();
			}
		} else {
			startDrag(transform, params.point);
		}
	}
}

void TransformTool::motion(const MotionParams &params)
{
	m_clickDetector.motion(params.viewPos);
	if(m_dragHandle != Handle::None) {
		canvas::TransformModel *transform = getActiveTransformModel();
		if(transform) {
			m_dragCurrentPoint = params.point;
			continueDrag(
				transform, params.constrain ^ m_constrain,
				params.center ^ m_center);
		}
	}
}

void TransformTool::modify(const ModifyParams &params)
{
	if(m_dragHandle != Handle::None) {
		canvas::TransformModel *transform = getActiveTransformModel();
		if(transform) {
			continueDrag(
				transform, params.constrain ^ m_constrain,
				params.center ^ m_center);
		}
	}
}

void TransformTool::hover(const HoverParams &params)
{
	m_angle = params.angle;
	m_zoom = params.zoom;
	m_swapDiagonal = params.mirror != params.flip;
	canvas::TransformModel *transform = getActiveTransformModel();
	if(transform) {
		updateHoverHandle(transform, params.point);
	}
}

void TransformTool::end(const EndParams &params)
{
	m_clickDetector.end();
	canvas::TransformModel *transform = getActiveTransformModel();
	if(transform && isDragging()) {
		int clicks = m_clickDetector.clicks();
		bool finish = m_applyOnEnd;
		if(!finish && clicks > 0) {
			// If this is the click that started the transform, don't act on it.
			if(m_firstClick) {
				m_doubleClickHandle = Handle::None;
			} else {
				if(m_dragHandle == Handle::Inside) {
					// User clicked into the transform, switch modes.
					m_mode =
						m_mode == Mode::Scale ? Mode::Distort : Mode::Scale;
				} else if(
					m_dragHandle == Handle::Outside &&
					((m_doubleClickHandle == Handle::Outside && clicks > 1) ||
					 m_invertMode)) {
					// User double-clicked or right-clicked outside of the
					// transform, apply it.
					finish = true;
				} else {
					// User clicked somewhere else, just disregard that.
				}
				m_doubleClickHandle = m_dragHandle;
			}
			transform->setDstQuad(m_quadStack[m_quadStackTop]);
		} else {
			continueDrag(
				transform, params.constrain ^ m_constrain,
				params.center ^ m_center);
			const TransformQuad &dstQuad = transform->dstQuad();
			if(dstQuad != m_dragStartQuad) {
				pushQuad(transform);
				m_doubleClickHandle = Handle::None;
				m_clickDetector.clear();
			}
		}

		m_firstClick = false;
		m_applyOnEnd = false;
		m_dragHandle = Handle::None;
		m_invertMode = false;
		emitStateChange(transform, m_hoverHandle);

		if(finish) {
			finishMultipart();
		}
	}
}

void TransformTool::finishMultipart()
{
	canvas::TransformModel *transform = getActiveTransformModel();
	bool allowed = true;
	if(transform && (allowed = transform->isAllowedToApplyActiveTransform())) {
		net::Client *client = m_owner.client();
		bool movedSelection = false;
		net::MessageList msgs = transform->applyActiveTransform(
			client->myId(), m_owner.activeLayer(),
			m_owner.transformInterpolation(), false, &movedSelection);
		allowed = checkAndSend(client, msgs);
		if(allowed) {
			endTransform(transform, !msgs.isEmpty() && movedSelection);
		}
	}

	if(allowed) {
		returnToPreviousTool();
	} else {
		emit m_owner.showMessageRequested(QCoreApplication::translate(
			"tools::TransformSettings",
			"You don't have permission for that transformation."));
	}
}

void TransformTool::cancelMultipart()
{
	canvas::TransformModel *transform = getActiveTransformModel();
	if(transform) {
		endTransform(transform, false);
	}
	returnToPreviousTool();
}

void TransformTool::undoMultipart()
{
	canvas::TransformModel *transform = getActiveTransformModel();
	if(transform) {
		if(m_quadStackTop > 0) {
			--m_quadStackTop;
			transform->setDstQuad(m_quadStack[m_quadStackTop]);
		} else {
			cancelMultipart();
		}
	}
}

void TransformTool::redoMultipart()
{
	canvas::TransformModel *transform = getActiveTransformModel();
	if(transform && m_quadStackTop + 1 < m_quadStack.size()) {
		++m_quadStackTop;
		transform->setDstQuad(m_quadStack[m_quadStackTop]);
	}
}

bool TransformTool::isMultipart() const
{
	return isTransformActive();
}

void TransformTool::offsetActiveTool(int x, int y)
{
	canvas::TransformModel *transform = getActiveTransformModel();
	if(transform) {
		transform->applyOffset(x, y);
		for(TransformQuad &quad : m_quadStack) {
			quad.applyOffset(-x, -y);
		}
	}
}

void TransformTool::beginTemporaryMove(
	Tool::Type toolToReturnTo, bool onlyMask, bool quickMove,
	const QCursor &outsideMoveCursor)
{
	m_outsideMoveCursor = outsideMoveCursor;
	if(!isTransformActive()) {
		m_toolToReturnTo = toolToReturnTo;
		tryBeginMove(false, onlyMask, quickMove ? Mode::Move : Mode::Scale);
	}
}

void TransformTool::beginTemporaryPaste(
	Tool::Type toolToReturnTo, const QRect &srcBounds, const QImage &image)
{
	if(!isTransformActive()) {
		m_toolToReturnTo = toolToReturnTo;
		tryBeginPaste(srcBounds, image);
	}
}

void TransformTool::clearTemporary()
{
	m_toolToReturnTo = Tool::Type::_LASTTOOL;
}

bool TransformTool::handleDeselect(bool finish)
{
	canvas::TransformModel *transform = getActiveTransformModel();
	if(transform) {
		transform->setDeselectOnApply(true);
		if(finish) {
			finishMultipart();
			return true;
		}
	}
	return false;
}

void TransformTool::setMode(Mode mode)
{
	canvas::TransformModel *transform = getActiveTransformModel();
	if(transform && mode != m_mode) {
		m_mode = mode;
		emitStateChange(
			transform,
			m_dragHandle == Handle::None ? m_hoverHandle : m_dragHandle);
	}
}

void TransformTool::mirror()
{
	canvas::TransformModel *transform = getActiveTransformModel();
	if(transform && m_dragHandle == Handle::None) {
		const TransformQuad &quad = transform->dstQuad();
		QPolygonF target({
			quad.topRight(),
			quad.topLeft(),
			quad.bottomLeft(),
			quad.bottomRight(),
		});
		updateQuad(transform, target);
		pushQuad(transform);
	}
}

void TransformTool::flip()
{
	canvas::TransformModel *transform = getActiveTransformModel();
	if(transform && m_dragHandle == Handle::None) {
		const TransformQuad &quad = transform->dstQuad();
		QPolygonF target({
			quad.bottomLeft(),
			quad.bottomRight(),
			quad.topRight(),
			quad.topLeft(),
		});
		updateQuad(transform, target);
		pushQuad(transform);
	}
}

void TransformTool::rotateCw()
{
	canvas::TransformModel *transform = getActiveTransformModel();
	if(transform && m_dragHandle == Handle::None) {
		updateQuad(transform, rotateQuad(transform->dstQuad(), 90.0));
		pushQuad(transform);
	}
}

void TransformTool::rotateCcw()
{
	canvas::TransformModel *transform = getActiveTransformModel();
	if(transform && m_dragHandle == Handle::None) {
		updateQuad(transform, rotateQuad(transform->dstQuad(), -90.0));
		pushQuad(transform);
	}
}

void TransformTool::shrinkToView(const QRectF &viewBounds)
{
	canvas::TransformModel *transform = getActiveTransformModel();
	if(transform && m_dragHandle == Handle::None) {
		QPolygonF target = transform->dstQuad().polygon();
		QRectF transformBounds = target.boundingRect();
		QSizeF transformSize = transformBounds.size();
		QSizeF viewSize = viewBounds.size() * 0.7;

		if(transformSize.width() > viewSize.width() ||
		   transformSize.height() > viewSize.height()) {
			QSizeF targetSize =
				transformSize.scaled(viewSize, Qt::KeepAspectRatio);
			QPointF center = transformBounds.center();
			QTransform t;
			t.translate(center.x(), center.y());
			t.scale(
				targetSize.width() / transformSize.width(),
				targetSize.height() / transformSize.height());
			t.translate(-center.x(), -center.y());
			target = t.map(target);
			transformBounds = target.boundingRect();
		}

		if(!transformBounds.intersects(viewBounds)) {
			QPointF offset = viewBounds.center() - transformBounds.center();
			target.translate(offset);
		}

		if(target != transform->dstQuad().polygon()) {
			updateQuad(transform, target);
			pushQuad(transform);
		}
	}
}


void TransformTool::stamp()
{
	canvas::TransformModel *transform = getActiveTransformModel();
	bool allowed = true;
	if(transform) {
		allowed =
			m_owner.model()->aclState()->canUseFeature(DP_FEATURE_PUT_IMAGE);
		if(allowed) {
			net::Client *client = m_owner.client();
			net::MessageList msgs = transform->applyActiveTransform(
				client->myId(), m_owner.activeLayer(),
				m_owner.transformInterpolation(), true);
			allowed = checkAndSend(client, msgs);
		}
	}

	if(!allowed) {
		emit m_owner.showMessageRequested(QCoreApplication::translate(
			"tools::TransformSettings",
			"You don't have permission to stamp selections."));
	}
}

TransformTool::Mode TransformTool::effectiveMode() const
{
	if(m_invertMode) {
		switch(m_mode) {
		case Mode::Scale:
			return Mode::Distort;
		case Mode::Distort:
			return Mode::Scale;
		default:
			break;
		}
	}
	return m_mode;
}

bool TransformTool::isTransformActive() const
{
	canvas::CanvasModel *canvas = m_owner.model();
	return canvas && canvas->transform()->isActive();
}

canvas::TransformModel *TransformTool::getActiveTransformModel() const
{
	canvas::CanvasModel *canvas = m_owner.model();
	if(canvas) {
		canvas::TransformModel *transform = canvas->transform();
		if(transform->isActive()) {
			return transform;
		}
	}
	return nullptr;
}

canvas::TransformModel *
TransformTool::tryBeginMove(bool firstClick, bool onlyMask, Mode mode)
{
	Q_ASSERT(!isTransformActive());
	Q_ASSERT(m_quadStack.isEmpty());

	canvas::CanvasModel *canvas = m_owner.model();
	if(!canvas) {
		emit m_owner.showMessageRequested(QCoreApplication::translate(
			"tools::TransformSettings", "No canvas present."));
		returnToPreviousTool();
		return nullptr;
	}

	if(!canvas->aclState()->canUseFeature(DP_FEATURE_REGION_MOVE)) {
		emit m_owner.showMessageRequested(QCoreApplication::translate(
			"tools::TransformSettings",
			"You don't have permission to transform selections."));
		returnToPreviousTool();
		return nullptr;
	}

	canvas::SelectionModel *selection = canvas->selection();
	if(!selection->isValid()) {
		emit m_owner.showMessageRequested(QCoreApplication::translate(
			"tools::TransformSettings",
			"Nothing selected that could be transformed."));
		returnToPreviousTool();
		return nullptr;
	}

	canvas::TransformModel *transform = canvas->transform();
	transform->beginFromCanvas(
		selection->bounds(), selection->mask(),
		onlyMask ? QSet<int>() : m_owner.selectedLayers());
	m_mode = mode;
	m_firstClick = firstClick;
	m_constrain = false;
	m_center = false;
	m_hoverHandle = Handle::Invalid;
	m_dragHandle = Handle::None;
	m_quadStack.clear();
	m_quadStack.append(transform->dstQuad());
	m_quadStackTop = 0;
	return transform;
}

void TransformTool::tryBeginPaste(const QRect &srcBounds, const QImage &image)
{
	Q_ASSERT(!isTransformActive());
	Q_ASSERT(m_quadStack.isEmpty());
	if(srcBounds.isEmpty() || image.isNull() || image.size().isEmpty()) {
		returnToPreviousTool();
		return;
	}

	canvas::CanvasModel *canvas = m_owner.model();
	if(!canvas) {
		returnToPreviousTool();
		return;
	}

	canvas::TransformModel *transform = canvas->transform();
	transform->beginFloating(srcBounds, image);
	m_mode = Mode::Scale;
	m_firstClick = false;
	m_applyOnEnd = false;
	m_constrain = false;
	m_center = false;
	m_hoverHandle = Handle::Invalid;
	m_dragHandle = Handle::None;
	m_quadStack.clear();
	m_quadStack.append(transform->dstQuad());
	m_quadStackTop = 0;
}

void TransformTool::endTransform(
	canvas::TransformModel *transform, bool applied)
{
	transform->endActiveTransform(applied);
	m_mode = Mode::Scale;
	m_hoverHandle = Handle::Invalid;
	m_dragHandle = Handle::None;
	m_doubleClickHandle = Handle::None;
	m_quadStack.clear();
	m_quadStackTop = -1;
	m_clickDetector.clear();
	emitStateChange(transform, Handle::None);
}

bool TransformTool::checkAndSend(
	net::Client *client, const net::MessageList &msgs)
{
	int count = msgs.size();
	if(count != 0) {
		canvas::CanvasModel *canvas = m_owner.model();
		if(canvas) {
			canvas::AclState *aclState = canvas->aclState();
			bool transformAllowed =
				aclState->canUseFeature(DP_FEATURE_REGION_MOVE);
			bool putImageAllowed =
				aclState->canUseFeature(DP_FEATURE_PUT_IMAGE);
			for(const net::Message &msg : msgs) {
				switch(msg.type()) {
				case DP_MSG_MOVE_RECT:
				case DP_MSG_MOVE_REGION:
				case DP_MSG_TRANSFORM_REGION:
					if(transformAllowed) {
						break;
					} else {
						return false;
					}
					break;
				case DP_MSG_PUT_IMAGE:
					if(putImageAllowed) {
						break;
					} else {
						return false;
					}
				default:
					break;
				}
			}
		}
		client->sendCommands(count, msgs.constData());
	}
	return true;
}

void TransformTool::updateHoverHandle(
	const canvas::TransformModel *transform, const QPointF &point)
{
	Handle hoverHandle = getHandleAt(transform, point, m_zoom);
	if(hoverHandle != m_hoverHandle) {
		m_hoverHandle = hoverHandle;
		if(!isDragging()) {
			emitStateChange(transform, m_hoverHandle);
		}
	}
}

void TransformTool::emitStateChange(
	const canvas::TransformModel *transform, Handle handle)
{
	emit m_owner.transformToolStateChanged(
		int(m_mode), int(handle), isDragging());
	setCursor(getHandleCursor(transform, handle));
}

QCursor TransformTool::getHandleCursor(
	const canvas::TransformModel *transform, Handle handle) const
{
	if(handle == Handle::None) {
		return Qt::PointingHandCursor;
	} else {
		Mode mode = effectiveMode();
		switch(mode) {
		case Mode::Scale:
			return getTransformHandleCursor(transform, handle);
		case Mode::Distort:
			return getDistortHandleCursor(transform, handle);
		case Mode::Move:
			return getMoveHandleCursor(handle);
		default:
			qWarning(
				"TransformTool::getHandleCursor: unknown mode %d", int(mode));
			return Qt::ArrowCursor;
		}
	}
}

QCursor TransformTool::getTransformHandleCursor(
	const canvas::TransformModel *transform, Handle handle) const
{
	switch(handle) {
	case Handle::TopLeft:
		return getArrowCursor(getCursorQuad(transform).topLeftAngle());
	case Handle::Top:
		return getArrowCursor(getCursorQuad(transform).topAngle());
	case Handle::TopRight:
		return getArrowCursor(getCursorQuad(transform).topRightAngle());
	case Handle::Right:
		return getArrowCursor(getCursorQuad(transform).rightAngle());
	case Handle::BottomRight:
		return getArrowCursor(getCursorQuad(transform).bottomRightAngle());
	case Handle::Bottom:
		return getArrowCursor(getCursorQuad(transform).bottomAngle());
	case Handle::BottomLeft:
		return getArrowCursor(getCursorQuad(transform).bottomLeftAngle());
	case Handle::Left:
		return getArrowCursor(getCursorQuad(transform).leftAngle());
	default:
		return getCommonHandleCursor(transform, handle);
	}
}

QCursor TransformTool::getDistortHandleCursor(
	const canvas::TransformModel *transform, Handle handle) const
{
	switch(handle) {
	case Handle::Top:
	case Handle::Bottom:
	case Handle::Left:
	case Handle::Right:
	case Handle::TopLeft:
	case Handle::BottomRight:
	case Handle::TopRight:
	case Handle::BottomLeft:
		return Qt::PointingHandCursor;
	default:
		return getCommonHandleCursor(transform, handle);
	}
}

QCursor TransformTool::getMoveHandleCursor(Handle handle) const
{
	if(handle == Handle::Inside) {
		return m_applyOnEnd ? Cursors::moveMask() : Cursors::move();
	} else {
		return m_outsideMoveCursor;
	}
}

QCursor TransformTool::getCommonHandleCursor(
	const canvas::TransformModel *transform, Handle handle) const
{
	switch(handle) {
	case Handle::TopEdge:
		return getShearCursor(getCursorQuad(transform).topEdgeAngle());
	case Handle::RightEdge:
		return getShearCursor(getCursorQuad(transform).rightEdgeAngle());
	case Handle::BottomEdge:
		return getShearCursor(getCursorQuad(transform).bottomEdgeAngle());
	case Handle::LeftEdge:
		return getShearCursor(getCursorQuad(transform).leftEdgeAngle());
	case Handle::Inside:
		return Cursors::move();
	case Handle::Outside:
		return Cursors::rotate();
	default:
		qWarning(
			"TransformTool::getCommonHandleCursor: invalid handle %d",
			int(handle));
		return Qt::ForbiddenCursor;
	}
}

QCursor TransformTool::getArrowCursor(qreal angle) const
{
	return getAngleCursor(
		angle, Qt::SizeHorCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor,
		Qt::SizeFDiagCursor);
}

QCursor TransformTool::getShearCursor(qreal angle) const
{
	return getAngleCursor(
		angle, Cursors::shearH(), Cursors::shearV(), Cursors::shearDiagB(),
		Cursors::shearDiagF());
}

QCursor TransformTool::getAngleCursor(
	qreal angle, const QCursor &cursorH, const QCursor &cursorV,
	const QCursor &cursorDiagB, const QCursor &cursorDiagF) const
{
	angle = std::fmod(angle - m_angle, 360.0);
	if(angle < 0.0) {
		angle += 360.0;
	}
	if((angle >= 22.5 && angle < 67.5) || (angle >= 202.5 && angle < 247.5)) {
		return m_swapDiagonal ? cursorDiagF : cursorDiagB;
	} else if(
		(angle >= 67.5 && angle < 112.5) || (angle >= 247.5 && angle < 292.5)) {
		return cursorV;
	} else if(
		(angle >= 112.5 && angle < 157.5) ||
		(angle >= 292.5 && angle < 337.5)) {
		return m_swapDiagonal ? cursorDiagB : cursorDiagF;
	} else {
		return cursorH;
	}
}


const TransformQuad &
TransformTool::getCursorQuad(const canvas::TransformModel *transform) const
{
	return isDragging() ? m_dragStartQuad : transform->dstQuad();
}

void TransformTool::startDrag(
	const canvas::TransformModel *transform, const QPointF &point)
{
	m_dragHandle =
		m_forceMove ? Handle::Inside
					: getHandleAt(transform, point, m_zoom, &m_dragStartOffset);
	if(m_dragHandle != Handle::None) {
		m_dragStartPoint = point;
		m_dragCurrentPoint = point;
		m_dragStartQuad = transform->dstQuad();
		m_dragStartHandlePoint =
			getQuadHandlePoint(m_dragStartQuad, m_dragHandle, m_dragStartPoint);
		emitStateChange(transform, m_dragHandle);
	}
}

void TransformTool::continueDrag(
	canvas::TransformModel *transform, bool constrain, bool center)
{
	Mode mode = effectiveMode();
	switch(mode) {
	case Mode::Scale:
		continueDragTransform(transform, constrain, center);
		break;
	case Mode::Distort:
		continueDragDistort(transform, constrain);
		break;
	case Mode::Move:
		if(m_dragHandle == Handle::Inside) {
			dragMove(transform, constrain);
		}
		break;
	default:
		qWarning("TransformTool::continueDrag: unknown mode %d", int(mode));
		break;
	}
}

void TransformTool::continueDragTransform(
	canvas::TransformModel *transform, bool constrain, bool center)
{
	switch(m_dragHandle) {
	case Handle::Top:
	case Handle::Right:
	case Handle::Bottom:
	case Handle::Left:
		dragScaleEdge(transform, center);
		break;
	case Handle::TopLeft:
	case Handle::TopRight:
	case Handle::BottomRight:
	case Handle::BottomLeft:
		dragScaleCorner(transform, constrain, center);
		break;
	case Handle::TopEdge:
	case Handle::RightEdge:
	case Handle::BottomEdge:
	case Handle::LeftEdge:
		dragShear(transform);
		break;
	case Handle::Inside:
		dragMove(transform, constrain);
		break;
	case Handle::Outside:
		dragRotate(transform, constrain);
		break;
	default:
		qWarning(
			"TransformTool::continueDragTransform: invalid handle %d",
			int(m_dragHandle));
	}
}

void TransformTool::continueDragDistort(
	canvas::TransformModel *transform, bool constrain)
{
	switch(m_dragHandle) {
	case Handle::TopLeft:
	case Handle::Top:
	case Handle::TopRight:
	case Handle::Right:
	case Handle::BottomRight:
	case Handle::Bottom:
	case Handle::BottomLeft:
	case Handle::Left:
		dragDistort(transform, constrain);
		break;
	case Handle::TopEdge:
	case Handle::RightEdge:
	case Handle::BottomEdge:
	case Handle::LeftEdge:
		dragShear(transform);
		break;
	case Handle::Inside:
		dragMove(transform, constrain);
		break;
	case Handle::Outside:
		dragRotate(transform, constrain);
		break;
	default:
		qWarning(
			"TransformTool::continueDragDistort: invalid handle %d",
			int(m_dragHandle));
	}
}

void TransformTool::dragMove(canvas::TransformModel *transform, bool constrain)
{
	QPointF delta = m_dragCurrentPoint - m_dragStartPoint;
	if(constrain) {
		if(std::abs(delta.x()) < std::abs(delta.y())) {
			delta.setX(0.0);
		} else {
			delta.setY(0.0);
		}
	}
	updateQuad(transform, m_dragStartQuad.polygon().translated(delta));
}

void TransformTool::dragRotate(
	canvas::TransformModel *transform, bool constrain)
{
	QPointF dragStartCenter = m_dragStartQuad.center();
	qreal a1 = QLineF(dragStartCenter, m_dragStartPoint).angle();
	qreal a2 = QLineF(dragStartCenter, m_dragCurrentPoint).angle();
	qreal ad = a1 - a2;
	if(constrain) {
		constexpr qreal STEP = 15.0;
		ad = std::round(ad / STEP) * STEP;
	}
	updateQuad(
		transform, rotateQuadAround(m_dragStartQuad, ad, dragStartCenter));
}

void TransformTool::dragShear(canvas::TransformModel *transform)
{
	QPointF delta = m_dragCurrentPoint - m_dragStartPoint;
	QPointF topLeftDelta, topRightDelta, bottomRightDelta, bottomLeftDelta;
	switch(m_dragHandle) {
	case Handle::TopEdge:
		delta = getConstrainedEdgeDelta(
			m_dragStartQuad.topLeft(), m_dragStartQuad.topRight(), delta);
		topLeftDelta = delta;
		topRightDelta = delta;
		bottomRightDelta = -delta;
		bottomLeftDelta = -delta;
		break;
	case Handle::RightEdge:
		delta = getConstrainedEdgeDelta(
			m_dragStartQuad.topRight(), m_dragStartQuad.bottomRight(), delta);
		topLeftDelta = -delta;
		topRightDelta = delta;
		bottomRightDelta = delta;
		bottomLeftDelta = -delta;
		break;
	case Handle::BottomEdge:
		delta = getConstrainedEdgeDelta(
			m_dragStartQuad.bottomRight(), m_dragStartQuad.bottomLeft(), delta);
		topLeftDelta = -delta;
		topRightDelta = -delta;
		bottomRightDelta = delta;
		bottomLeftDelta = delta;
		break;
	case Handle::LeftEdge:
		delta = getConstrainedEdgeDelta(
			m_dragStartQuad.bottomLeft(), m_dragStartQuad.topLeft(), delta);
		topLeftDelta = delta;
		topRightDelta = -delta;
		bottomRightDelta = -delta;
		bottomLeftDelta = delta;
		break;
	default:
		qWarning(
			"TransformTool::dragShear: invalid handle %d", int(m_dragHandle));
		return;
	}
	QPolygonF target({
		m_dragStartQuad.topLeft() + topLeftDelta,
		m_dragStartQuad.topRight() + topRightDelta,
		m_dragStartQuad.bottomRight() + bottomRightDelta,
		m_dragStartQuad.bottomLeft() + bottomLeftDelta,
	});
	updateQuad(transform, target);
}

void TransformTool::dragScaleEdge(
	canvas::TransformModel *transform, bool center)
{
	qreal angle;
	switch(m_dragHandle) {
	case Handle::Top:
		angle = m_dragStartQuad.topAngle();
		break;
	case Handle::Right:
		angle = m_dragStartQuad.rightAngle();
		break;
	case Handle::Bottom:
		angle = m_dragStartQuad.bottomAngle();
		break;
	case Handle::Left:
		angle = m_dragStartQuad.leftAngle();
		break;
	default:
		qWarning(
			"TransformTool::dragScale: invalid handle %d", int(m_dragHandle));
		return;
	}

	QLineF axis = QLineF::fromPolar(1.0, angle).translated(m_dragStartPoint);
	QLineF cross =
		QLineF::fromPolar(1.0, angle + 90.0).translated(m_dragCurrentPoint);
	QPointF pointOnAxis;
	if(axis.intersects(cross, &pointOnAxis) == QLineF::NoIntersection) {
		qWarning("TransformTool::dragScale: no point on axis");
		return;
	}

	QPointF delta = pointOnAxis - m_dragStartPoint;
	QPointF topLeftDelta, topRightDelta, bottomRightDelta, bottomLeftDelta;
	switch(m_dragHandle) {
	case Handle::Top:
		topLeftDelta = delta;
		topRightDelta = delta;
		if(center) {
			bottomRightDelta = -delta;
			bottomLeftDelta = -delta;
		}
		break;
	case Handle::Right:
		topRightDelta = delta;
		bottomRightDelta = delta;
		if(center) {
			topLeftDelta = -delta;
			bottomLeftDelta = -delta;
		}
		break;
	case Handle::Bottom:
		bottomRightDelta = delta;
		bottomLeftDelta = delta;
		if(center) {
			topLeftDelta = -delta;
			topRightDelta = -delta;
		}
		break;
	case Handle::Left:
		topLeftDelta = delta;
		bottomLeftDelta = delta;
		if(center) {
			topRightDelta = -delta;
			bottomRightDelta = -delta;
		}
		break;
	default:
		qWarning(
			"TransformTool::dragScale: invalid handle %d", int(m_dragHandle));
		return;
	}

	QPolygonF target({
		m_dragStartQuad.topLeft() + topLeftDelta,
		m_dragStartQuad.topRight() + topRightDelta,
		m_dragStartQuad.bottomRight() + bottomRightDelta,
		m_dragStartQuad.bottomLeft() + bottomLeftDelta,
	});
	updateQuad(transform, target);
}

void TransformTool::dragScaleCorner(
	canvas::TransformModel *transform, bool constrain, bool center)
{
	int dragged, dependent1, dependent2, opposite;
	switch(m_dragHandle) {
	case Handle::TopLeft:
		dragged = TransformQuad::TOP_LEFT;
		dependent1 = TransformQuad::TOP_RIGHT;
		dependent2 = TransformQuad::BOTTOM_LEFT;
		opposite = TransformQuad::BOTTOM_RIGHT;
		break;
	case Handle::TopRight:
		dragged = TransformQuad::TOP_RIGHT;
		dependent1 = TransformQuad::BOTTOM_RIGHT;
		dependent2 = TransformQuad::TOP_LEFT;
		opposite = TransformQuad::BOTTOM_LEFT;
		break;
	case Handle::BottomRight:
		dragged = TransformQuad::BOTTOM_RIGHT;
		dependent1 = TransformQuad::BOTTOM_LEFT;
		dependent2 = TransformQuad::TOP_RIGHT;
		opposite = TransformQuad::TOP_LEFT;
		break;
	case Handle::BottomLeft:
		dragged = TransformQuad::BOTTOM_LEFT;
		dependent1 = TransformQuad::TOP_LEFT;
		dependent2 = TransformQuad::BOTTOM_RIGHT;
		opposite = TransformQuad::TOP_RIGHT;
		break;
	default:
		qWarning(
			"TransformTool::dragScaleCorner: invalid handle %d",
			int(m_dragHandle));
		return;
	}

	QPointF draggedPoint = m_dragStartQuad.at(dragged);
	QPointF dependentPoint1 = m_dragStartQuad.at(dependent1);
	QPointF dependentPoint2 = m_dragStartQuad.at(dependent2);
	QPointF oppositePoint = m_dragStartQuad.at(opposite);

	QPointF delta = m_dragCurrentPoint - m_dragStartPoint;
	if(constrain) {
		delta = getPointAlongAxis(
					QLineF(m_dragStartHandlePoint, oppositePoint),
					m_dragStartHandlePoint + delta) -
				m_dragStartHandlePoint;
	}

	QLineF axis1(dependentPoint1, oppositePoint);
	QLineF axis2(dependentPoint2, oppositePoint);
	if(center) {
		axis1.translate(-delta);
		axis2.translate(-delta);
	}
	QLineF cross1 = QLineF(draggedPoint, dependentPoint1).translated(delta);
	QLineF cross2 = QLineF(draggedPoint, dependentPoint2).translated(delta);
	QPointF pointOnAxis1;
	QPointF pointOnAxis2;
	if(axis1.intersects(cross1, &pointOnAxis1) == QLineF::NoIntersection ||
	   axis2.intersects(cross2, &pointOnAxis2) == QLineF::NoIntersection) {
		qWarning("TransformTool::dragScaleCorner: no point on axis");
		return;
	}

	QPointF deltas[4];
	deltas[dragged] = delta;
	deltas[dependent1] = pointOnAxis1 - dependentPoint1;
	deltas[dependent2] = pointOnAxis2 - dependentPoint2;
	if(center) {
		deltas[opposite] = -delta;
	}

	QPolygonF target({
		m_dragStartQuad.topLeft() + deltas[TransformQuad::TOP_LEFT],
		m_dragStartQuad.topRight() + deltas[TransformQuad::TOP_RIGHT],
		m_dragStartQuad.bottomRight() + deltas[TransformQuad::BOTTOM_RIGHT],
		m_dragStartQuad.bottomLeft() + deltas[TransformQuad::BOTTOM_LEFT],
	});
	updateQuad(transform, target);
}

void TransformTool::dragDistort(
	canvas::TransformModel *transform, bool constrain)
{
	QPointF delta = m_dragCurrentPoint - m_dragStartPoint;
	QPointF topLeftDelta, topRightDelta, bottomRightDelta, bottomLeftDelta;
	switch(m_dragHandle) {
	case Handle::TopLeft:
		topLeftDelta = delta;
		break;
	case Handle::Top:
		if(constrain) {
			delta = getConstrainedEdgeDelta(
				m_dragStartQuad.topLeft(), m_dragStartQuad.topRight(), delta);
		}
		topLeftDelta = delta;
		topRightDelta = delta;
		break;
	case Handle::TopRight:
		topRightDelta = delta;
		break;
	case Handle::Right:
		if(constrain) {
			delta = getConstrainedEdgeDelta(
				m_dragStartQuad.topRight(), m_dragStartQuad.bottomRight(),
				delta);
		}
		topRightDelta = delta;
		bottomRightDelta = delta;
		break;
	case Handle::BottomRight:
		bottomRightDelta = delta;
		break;
	case Handle::Bottom:
		if(constrain) {
			delta = getConstrainedEdgeDelta(
				m_dragStartQuad.bottomRight(), m_dragStartQuad.bottomLeft(),
				delta);
		}
		bottomRightDelta = delta;
		bottomLeftDelta = delta;
		break;
	case Handle::BottomLeft:
		bottomLeftDelta = delta;
		break;
	case Handle::Left:
		if(constrain) {
			delta = getConstrainedEdgeDelta(
				m_dragStartQuad.bottomLeft(), m_dragStartQuad.topLeft(), delta);
		}
		bottomLeftDelta = delta;
		topLeftDelta = delta;
		break;
	default:
		qWarning(
			"TransformTool::dragDistort: invalid handle %d", int(m_dragHandle));
		return;
	}
	QPolygonF target({
		m_dragStartQuad.topLeft() + topLeftDelta,
		m_dragStartQuad.topRight() + topRightDelta,
		m_dragStartQuad.bottomRight() + bottomRightDelta,
		m_dragStartQuad.bottomLeft() + bottomLeftDelta,
	});
	updateQuad(transform, target);
}

QPointF TransformTool::getConstrainedEdgeDelta(
	const QPointF &prevPoint, const QPointF &nextPoint,
	const QPointF &delta) const
{
	return getPointAlongAxis(
			   QLineF(prevPoint, nextPoint), m_dragStartPoint + delta) -
		   m_dragStartPoint + m_dragStartOffset;
}

QPointF
TransformTool::getPointAlongAxis(const QLineF &axis, const QPointF &target)
{
	QLineF cross =
		QLineF::fromPolar(1.0, axis.angle() + 90.0).translated(target);
	QPointF pointOnAxis;
	if(axis.intersects(cross, &pointOnAxis) != QLineF::NoIntersection) {
		return pointOnAxis;
	} else {
		qWarning("TransformTool::getPointAlongAxis: no point on axis");
		return target;
	}
}

QPolygonF TransformTool::rotateQuad(const TransformQuad &quad, qreal angle)
{
	return rotateQuadAround(quad, angle, quad.center());
}

QPolygonF TransformTool::rotateQuadAround(
	const TransformQuad &quad, qreal angle, const QPointF &center)
{
	QTransform t;
	t.translate(center.x(), center.y());
	t.rotate(angle);
	t.translate(-center.x(), -center.y());
	return t.map(quad.polygon());
}

void TransformTool::updateQuad(
	canvas::TransformModel *transform, const QPolygonF &target)
{
	transform->setDstQuad(TransformQuad(target));
}

void TransformTool::pushQuad(canvas::TransformModel *transform)
{
	int popCount = m_quadStack.size() - m_quadStackTop - 1;
	if(popCount > 0) {
		m_quadStack.remove(m_quadStackTop + 1, popCount);
	}

	int shiftCount = (m_quadStack.size() + 1) - MAX_QUAD_STACK_DEPTH;
	if(shiftCount > 0) {
		m_quadStack.remove(0, shiftCount);
	}

	m_quadStack.append(transform->dstQuad());
	m_quadStackTop = m_quadStack.size() - 1;
}

TransformTool::Handle TransformTool::getHandleAt(
	const canvas::TransformModel *transform, const QPointF &point, qreal zoom,
	QPointF *outOffset) const
{
	Handle closestHandle = Handle::None;
	QPointF offset;
	if(transform && zoom > 0.0) {
		const TransformQuad &quad = transform->dstQuad();
		if(m_mode != Mode::Move) {
			qreal closestDistance;
			checkHandleAt(
				point, zoom, closestHandle, closestDistance, offset,
				Handle::TopLeft, quad.topLeft());
			checkHandleAt(
				point, zoom, closestHandle, closestDistance, offset,
				Handle::Top, quad.top());
			checkHandleAt(
				point, zoom, closestHandle, closestDistance, offset,
				Handle::TopRight, quad.topRight());
			checkHandleAt(
				point, zoom, closestHandle, closestDistance, offset,
				Handle::Right, quad.right());
			checkHandleAt(
				point, zoom, closestHandle, closestDistance, offset,
				Handle::BottomRight, quad.bottomRight());
			checkHandleAt(
				point, zoom, closestHandle, closestDistance, offset,
				Handle::Bottom, quad.bottom());
			checkHandleAt(
				point, zoom, closestHandle, closestDistance, offset,
				Handle::BottomLeft, quad.bottomLeft());
			checkHandleAt(
				point, zoom, closestHandle, closestDistance, offset,
				Handle::Left, quad.left());
			if(closestHandle == Handle::None) {
				checkHandleEdgeAt(
					point, zoom, closestHandle, closestDistance, offset,
					Handle::TopEdge, QLineF(quad.topLeft(), quad.topRight()));
				checkHandleEdgeAt(
					point, zoom, closestHandle, closestDistance, offset,
					Handle::RightEdge,
					QLineF(quad.topRight(), quad.bottomRight()));
				checkHandleEdgeAt(
					point, zoom, closestHandle, closestDistance, offset,
					Handle::BottomEdge,
					QLineF(quad.bottomRight(), quad.bottomLeft()));
				checkHandleEdgeAt(
					point, zoom, closestHandle, closestDistance, offset,
					Handle::LeftEdge,
					QLineF(quad.bottomLeft(), quad.topLeft()));
			}
		}
		if(closestHandle == Handle::None) {
			closestHandle =
				quad.containsPoint(point) ? Handle::Inside : Handle::Outside;
			offset = QPointF(0.0, 0.0);
		}
	}
	if(outOffset) {
		*outOffset = offset;
	}
	return closestHandle;
}

void TransformTool::checkHandleAt(
	const QPointF &point, qreal zoom, Handle &inOutClosestHandle,
	qreal &inOutClosestDistance, QPointF &outOffset, Handle consideredHandle,
	const QPointF &consideredPoint) const
{
	qreal size = HANDLE_SIZE / zoom;
	qreal halfSize = size / 2.0;
	QRectF hitRect(
		consideredPoint - QPointF(halfSize, halfSize), QSizeF(size, size));
	if(hitRect.contains(point)) {
		qreal distance = QLineF(point, consideredPoint).length();
		if(inOutClosestHandle == Handle::None ||
		   distance < inOutClosestDistance) {
			inOutClosestHandle = consideredHandle;
			inOutClosestDistance = distance;
			outOffset = point - consideredPoint;
		}
	}
}

void TransformTool::checkHandleEdgeAt(
	const QPointF &point, qreal zoom, Handle &inOutClosestHandle,
	qreal &inOutClosestDistance, QPointF &outOffset, Handle consideredHandle,
	const QLineF &consideredEdge) const
{
	QPointF edgePoint = getPointAlongAxis(consideredEdge, point);
	qreal distance = QLineF(point, edgePoint).length();
	qreal size = HANDLE_SIZE / zoom;
	qreal halfSize = size / 2.0;
	if( // Are we close enough to the axis?
		distance < halfSize &&
		// Is this the closest candidate?
		(inOutClosestHandle == Handle::None ||
		 distance < inOutClosestDistance) &&
		// Is our point actually along the edge, not just the infinite axis?
		QLineF::fromPolar(size, QLineF(point, edgePoint).angle())
				.translated(point)
				.intersects(consideredEdge, nullptr) ==
			QLineF::BoundedIntersection) {
		inOutClosestHandle = consideredHandle;
		inOutClosestDistance = distance;
		outOffset = point - edgePoint;
	}
}

QPointF TransformTool::getQuadHandlePoint(
	const TransformQuad &quad, Handle handle, const QPointF &targetPoint)
{
	switch(handle) {
	case Handle::TopLeft:
		return quad.topLeft();
	case Handle::Top:
		return quad.top();
	case Handle::TopRight:
		return quad.topRight();
	case Handle::Right:
		return quad.right();
	case Handle::BottomRight:
		return quad.bottomRight();
	case Handle::Bottom:
		return quad.bottom();
	case Handle::BottomLeft:
		return quad.bottomLeft();
	case Handle::Left:
		return quad.left();
	default:
		return targetPoint;
	}
}

void TransformTool::returnToPreviousTool()
{
	if(m_toolToReturnTo != Tool::Type::_LASTTOOL) {
		Tool::Type toolToReturnTo = m_toolToReturnTo;
		m_toolToReturnTo = Tool::Type::_LASTTOOL;
		emit m_owner.toolSwitchRequested(toolToReturnTo);
	}
}

}
