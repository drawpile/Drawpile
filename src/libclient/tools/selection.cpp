// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/selection.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/selectionmodel.h"
#include "libclient/net/client.h"
#include "libclient/utils/cursors.h"
#include <QDateTime>
#include <QPainter>
#include <QPainterPath>
#include <functional>

using std::placeholders::_1;
using utils::Cursors;

namespace tools {

SelectionTool::SelectionTool(ToolController &owner, Type type, QCursor cursor)
	: Tool(
		  owner, type, cursor,
		  Capability::AllowColorPick | Capability::Fractional)
{
}

void SelectionTool::begin(const BeginParams &params)
{
	m_clickDetector.begin(params.viewPos, params.deviceType);
	m_zoom = params.zoom;
	m_lastPoint = params.point;
	bool atEdge;
	if(params.right) {
		m_op = -1;
		setCursor(
			getCursor(resolveOp(params.constrain, params.center, defaultOp())));
	} else if(
		!m_forceSelect && !params.constrain && !params.center && quickDrag() &&
		(isInsideSelection(params.point, &atEdge) || atEdge)) {
		m_op = -1;
		m_startPoint = params.point;
		m_owner.requestTransformMove(params, atEdge, true);
		setCursor(
			getCursor(resolveOp(params.constrain, params.center, defaultOp())));
	} else {
		m_op = resolveOp(params.constrain, params.center, defaultOp());
		m_startPoint = params.point;
		m_wasForceSelect = m_forceSelect;
		beginSelection(params.point);
		setCursor(getCursor(m_op));
	}
}

void SelectionTool::motion(const MotionParams &params)
{
	m_clickDetector.motion(params.viewPos);
	m_lastPoint = params.point;
	if(m_op != -1) {
		m_op = resolveOp(params.constrain, params.center, defaultOp());
		continueSelection(params.point);
	}
	updateCursor(params.point, params.constrain, params.center);
}

void SelectionTool::modify(const ModifyParams &params)
{
	if(m_op != -1) {
		m_op = resolveOp(params.constrain, params.center, defaultOp());
	}
	updateCursor(m_lastPoint, params.constrain, params.center);
}

void SelectionTool::hover(const HoverParams &params)
{
	m_zoom = params.zoom;
	m_lastPoint.setX(params.point.x());
	m_lastPoint.setY(params.point.y());
	updateCursor(params.point, params.constrain, params.center);
}

void SelectionTool::end(const EndParams &params)
{
	if(m_op != -1) {
		m_op = resolveOp(params.constrain, params.center, defaultOp());
	}
	endSelection(true, params.center);
	updateCursor(m_lastPoint, params.constrain, params.center);
}

void SelectionTool::finishMultipart()
{
	endSelection(false, false);
}

void SelectionTool::cancelMultipart()
{
	if(m_op != -1) {
		cancelSelection();
		m_op = -1;
	}
}

void SelectionTool::undoMultipart()
{
	if(m_op != -1) {
		cancelSelection();
		m_op = -1;
	}
}

bool SelectionTool::isMultipart() const
{
	return m_op != -1;
}

void SelectionTool::offsetActiveTool(int x, int y)
{
	if(m_op != -1) {
		QPoint offset(x, y);
		m_startPoint += offset;
		offsetSelection(offset);
	}
}

int SelectionTool::resolveOp(bool constrain, bool center, int defaultOp)
{
	int op;
	if(constrain) {
		if(center) {
			op = DP_MSG_SELECTION_PUT_OP_INTERSECT;
		} else {
			op = DP_MSG_SELECTION_PUT_OP_UNITE;
		}
	} else if(center) {
		op = DP_MSG_SELECTION_PUT_OP_EXCLUDE;
	} else {
		return defaultOp;
	}
	return op == defaultOp ? DP_MSG_SELECTION_PUT_OP_REPLACE : op;
}

bool SelectionTool::quickDrag() const
{
	return m_owner.selectionParams().dragMode !=
		   int(ToolController::SelectionDragMode::Select);
}

void SelectionTool::updateSelectionPreview(const QPainterPath &path) const
{
	emit m_owner.pathPreviewRequested(path);
}

void SelectionTool::removeSelectionPreview() const
{
	updateSelectionPreview(QPainterPath());
}

void SelectionTool::updateCursor(
	const QPointF &point, bool constrain, bool center)
{
	bool atEdge;
	if(!constrain && !center && m_op == -1 && !m_forceSelect &&
	   !m_wasForceSelect && quickDrag() &&
	   (isInsideSelection(point, &atEdge) || atEdge)) {
		setCursor(atEdge ? Cursors::moveMask() : Cursors::move());
	} else {
		setCursor(getCursor(
			m_op == -1 ? resolveOp(constrain, center, defaultOp()) : m_op));
	}
}

void SelectionTool::endSelection(bool click, bool onlyMask)
{
	m_clickDetector.end();
	if(m_op != -1) {
		bool isClick = click && m_clickDetector.isClick();
		bool atEdge = false;
		if(isClick && !m_wasForceSelect &&
		   (quickDrag() ? isInsideSelection(startPoint(), &atEdge) || atEdge
						: isInsideSelection(startPoint()))) {
			cancelSelection();
			emit m_owner.transformRequested(onlyMask || atEdge, false, false);
		} else {
			net::Client *client = m_owner.client();
			uint8_t contextId = client->myId();
			net::MessageList msgs = m_op != defaultOp() || !isClick
										? endSelection(contextId)
										: endDeselection(contextId);
			if(!msgs.isEmpty()) {
				msgs.prepend(net::makeUndoPointMessage(contextId));
				client->sendCommands(msgs.size(), msgs.constData());
			}
		}
		m_op = -1;
	}
}

net::MessageList SelectionTool::endDeselection(uint8_t contextId)
{
	cancelSelection();
	return {net::makeSelectionClearMessage(
		contextId, canvas::CanvasModel::MAIN_SELECTION_ID)};
}

bool SelectionTool::isInsideSelection(const QPointF &point, bool *atEdge) const
{
	canvas::CanvasModel *canvas = m_owner.model();
	if(canvas) {
		canvas::SelectionModel *selection = canvas->selection();
		if(selection->isValid()) {
			const QRect &bounds = selection->bounds();
			const QImage &mask = selection->image();
			QPoint boundsPos = bounds.topLeft();
			auto isInside = [&](const QPoint &pos) {
				return bounds.contains(pos) &&
					   qAlpha(mask.pixel(pos - boundsPos)) != 0;
			};

			QPoint p = point.toPoint();
			bool inside = isInside(p);
			if(atEdge) {
				qreal length = m_zoom <= 0.0 ? EDGE_SLOP : EDGE_SLOP / m_zoom;
				bool edgeFound = false;
				auto compareAtAngle = [&](int a) {
					QPoint q =
						(point + QLineF::fromPolar(length, qreal(a)).p2())
							.toPoint();
					return q != p && isInside(q) != inside;
				};
				// Determine if we're at an edge by sampling points around the
				// cursor. If we're inside the selection and find a point
				// outside of it or vice-versa, we consider it an edge. An
				// exception here is if the cursor is inside the selection and
				// there's two points opposite of each other that are outside,
				// this must be a very narrow selection and isn't an edge.
				if(inside) {
					for(int angle = 0; angle < 180; angle += 45) {
						if(compareAtAngle(angle)) {
							if(compareAtAngle(angle + 180)) {
								edgeFound = false;
								break;
							} else {
								edgeFound = true;
							}
						} else if(compareAtAngle(angle + 180)) {
							edgeFound = true;
						}
					}
				} else {
					for(int angle = 0; angle < 360; angle += 45) {
						if(compareAtAngle(angle)) {
							edgeFound = true;
							break;
						}
					}
				}
				*atEdge = edgeFound;
			}
			return inside;
		}
	}
	if(atEdge) {
		*atEdge = false;
	}
	return false;
}


RectangleSelection::RectangleSelection(ToolController &owner)
	: SelectionTool(owner, SELECTION, Cursors::selectRectangle())
{
}

const QCursor &RectangleSelection::getCursor(int effectiveOp) const
{
	switch(effectiveOp) {
	case DP_MSG_SELECTION_PUT_OP_UNITE:
		return Cursors::selectRectangleUnite();
	case DP_MSG_SELECTION_PUT_OP_INTERSECT:
		return Cursors::selectRectangleIntersect();
	case DP_MSG_SELECTION_PUT_OP_EXCLUDE:
		return Cursors::selectRectangleExclude();
	default:
		return Cursors::selectRectangle();
	}
}

void RectangleSelection::beginSelection(const canvas::Point &point)
{
	m_end = point;
	updateRectangleSelectionPreview();
}

void RectangleSelection::continueSelection(const canvas::Point &point)
{
	m_end = point;
	updateRectangleSelectionPreview();
}

void RectangleSelection::offsetSelection(const QPoint &offset)
{
	m_end += offset;
	updateRectangleSelectionPreview();
}

void RectangleSelection::cancelSelection()
{
	removeSelectionPreview();
}

net::MessageList RectangleSelection::endSelection(uint8_t contextId)
{
	removeSelectionPreview();

	QRect area;
	QImage mask;
	if(antiAlias()) {
		QRectF rect = getRectF();
		if(!rect.isEmpty()) {
			area = rect.toAlignedRect();
			mask = QImage(area.size(), QImage::Format_ARGB32_Premultiplied);
			mask.fill(0);
			QPainter painter(&mask);
			painter.setPen(Qt::NoPen);
			painter.setBrush(Qt::black);
			painter.setRenderHint(QPainter::Antialiasing, true);
			painter.drawRect(
				QRectF(rect.topLeft() - QPointF(area.topLeft()), rect.size()));
		}
	} else {
		area = getRect();
	}

	if(area.isEmpty()) {
		return {net::makeSelectionClearMessage(
			contextId, canvas::CanvasModel::MAIN_SELECTION_ID)};
	} else {
		net::MessageList msgs;
		net::makeSelectionPutMessages(
			msgs, contextId, canvas::CanvasModel::MAIN_SELECTION_ID, op(),
			area.x(), area.y(), area.width(), area.height(), mask);
		return msgs;
	}
}

void RectangleSelection::updateRectangleSelectionPreview()
{
	QPainterPath path;
	if(antiAlias()) {
		QRectF rect = getRectF();
		if(!rect.isEmpty()) {
			path.addRect(rect);
		}
	} else {
		QRect rect = getRect();
		if(!rect.isEmpty()) {
			path.addRect(rect);
		}
	}

	if(path.isEmpty()) {
		removeSelectionPreview();
	} else {
		updateSelectionPreview(path);
	}
}

QRect RectangleSelection::getRect() const
{
	return getRectF().toRect();
}

QRectF RectangleSelection::getRectF() const
{
	const QPointF &start = startPoint();
	return QRectF(
		QPointF(qMin(start.x(), m_end.x()), qMin(start.y(), m_end.y())),
		QPointF(qMax(start.x(), m_end.x()), qMax(start.y(), m_end.y())));
}


PolygonSelection::PolygonSelection(ToolController &owner)
	: SelectionTool(owner, POLYGONSELECTION, Cursors::selectLasso())
	, m_strokeEngine(
		  [this](DP_BrushPoint bp, const drawdance::CanvasState &) {
			  addPoint(QPointF(bp.x, bp.y));
		  },
		  std::bind(&PolygonSelection::pollControl, this, _1))
{
	m_pollTimer.setSingleShot(false);
	m_pollTimer.setTimerType(Qt::PreciseTimer);
	m_pollTimer.setInterval(15);
	QObject::connect(&m_pollTimer, &QTimer::timeout, [this]() {
		poll();
	});
}

const QCursor &PolygonSelection::getCursor(int effectiveOp) const
{
	switch(effectiveOp) {
	case DP_MSG_SELECTION_PUT_OP_UNITE:
		return Cursors::selectLassoUnite();
	case DP_MSG_SELECTION_PUT_OP_INTERSECT:
		return Cursors::selectLassoIntersect();
	case DP_MSG_SELECTION_PUT_OP_EXCLUDE:
		return Cursors::selectLassoExclude();
	default:
		return Cursors::selectLasso();
	}
}

void PolygonSelection::beginSelection(const canvas::Point &point)
{
	m_polygon.clear();
	m_polygonF.clear();
	m_lastTimeMsec = point.timeMsec();
	m_owner.setStrokeEngineParams(m_strokeEngine);
	m_strokeEngine.beginStroke();
	m_strokeEngine.strokeTo(point, drawdance::CanvasState::null());
}

void PolygonSelection::continueSelection(const canvas::Point &point)
{
	m_lastTimeMsec = point.timeMsec();
	m_strokeEngine.strokeTo(point, drawdance::CanvasState::null());
}

void PolygonSelection::offsetSelection(const QPoint &offset)
{
	if(antiAlias()) {
		m_polygonF.translate(offset);
	} else {
		m_polygon.translate(offset);
	}
	updatePolygonSelectionPreview();
}

void PolygonSelection::cancelSelection()
{
	m_strokeEngine.endStroke(m_lastTimeMsec, drawdance::CanvasState::null());
	removeSelectionPreview();
}

net::MessageList PolygonSelection::endSelection(uint8_t contextId)
{
	int pointCount = antiAlias() ? m_polygonF.size() : m_polygon.size();
	if(pointCount != 0) {
		m_strokeEngine.strokeTo(
			canvas::Point(
				m_lastTimeMsec,
				antiAlias() ? m_polygonF.first() : QPointF(m_polygon.first()),
				1.0),
			drawdance::CanvasState::null());
	}

	m_strokeEngine.endStroke(m_lastTimeMsec, drawdance::CanvasState::null());
	removeSelectionPreview();

	QRect area;
	if(pointCount >= 2) {
		if(antiAlias()) {
			QRectF bounds = m_polygonF.boundingRect();
			area = bounds.toAlignedRect();
			m_polygonF.translate(-QPointF(area.topLeft()));
		} else {
			area = m_polygon.boundingRect();
			m_polygon.translate(-area.topLeft());
		}
	}

	if(area.isEmpty()) {
		return {net::makeSelectionClearMessage(
			contextId, canvas::CanvasModel::MAIN_SELECTION_ID)};
	} else {
		QImage mask(area.size(), QImage::Format_ARGB32_Premultiplied);
		mask.fill(0);
		QPainter painter(&mask);
		painter.setPen(Qt::NoPen);
		painter.setBrush(Qt::black);
		painter.setRenderHint(QPainter::Antialiasing, antiAlias());
		if(antiAlias()) {
			painter.drawPolygon(m_polygonF);
		} else {
			painter.drawPolygon(m_polygon);
		}
		net::MessageList msgs;
		net::makeSelectionPutMessages(
			msgs, contextId, canvas::CanvasModel::MAIN_SELECTION_ID, op(),
			area.x(), area.y(), area.width(), area.height(), mask);
		return msgs;
	}
}

void PolygonSelection::addPoint(const QPointF &point)
{
	if(antiAlias()) {
		if(m_polygonF.isEmpty() ||
		   !canvas::Point::intSame(point, m_polygonF.last())) {
			m_polygonF.append(point);
			updatePolygonSelectionPreview();
		}
	} else {
		QPoint p = point.toPoint();
		if(m_polygon.isEmpty() || p != m_polygon.last()) {
			m_polygon.append(p);
			updatePolygonSelectionPreview();
		}
	}
}

void PolygonSelection::pollControl(bool enable)
{
	if(enable) {
		m_pollTimer.start();
	} else {
		m_pollTimer.stop();
	}
}

void PolygonSelection::poll()
{
	m_strokeEngine.poll(
		QDateTime::currentMSecsSinceEpoch(), drawdance::CanvasState::null());
}

void PolygonSelection::updatePolygonSelectionPreview()
{
	QPainterPath path;
	if(antiAlias()) {
		if(m_polygonF.size() >= 2) {
			path.addPolygon(m_polygonF);
		}
	} else {
		if(m_polygon.size() >= 2) {
			path.addPolygon(m_polygon);
		}
	}

	if(path.isEmpty()) {
		removeSelectionPreview();
	} else {
		updateSelectionPreview(path);
	}
}

}
