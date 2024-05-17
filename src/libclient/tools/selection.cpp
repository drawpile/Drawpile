// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/selection.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/selectionmodel.h"
#include "libclient/net/client.h"
#include <QPainter>
#include <QPainterPath>

namespace tools {

SelectionTool::SelectionTool(ToolController &owner, Type type, QCursor cursor)
	: Tool(owner, type, cursor, true, false, false, true, false)
{
}

void SelectionTool::begin(const BeginParams &params)
{
	m_clickDetector.begin(params.viewPos);
	if(params.right) {
		m_op = -1;
	} else {
		m_params = m_owner.selectionParams();
		m_op = defaultOp();
		m_startPoint = params.point;
		beginSelection(params.point);
	}
}

void SelectionTool::motion(const MotionParams &params)
{
	m_clickDetector.motion(params.viewPos);
	if(m_op != -1) {
		if(params.constrain) {
			if(params.center) {
				setOrRevertOp(DP_MSG_SELECTION_PUT_OP_INTERSECT);
			} else {
				setOrRevertOp(DP_MSG_SELECTION_PUT_OP_UNITE);
			}
		} else if(params.center) {
			setOrRevertOp(DP_MSG_SELECTION_PUT_OP_EXCLUDE);
		} else {
			m_op = defaultOp();
		}
		continueSelection(params.point);
	}
}

void SelectionTool::end()
{
	m_clickDetector.end();
	if(m_op != -1) {
		bool isClick = m_clickDetector.isClick();
		if(isClick && isInsideSelection(startPoint())) {
			cancelSelection();
			emit m_owner.transformRequested();
		} else {
			net::Client *client = m_owner.client();
			uint8_t contextId = client->myId();
			net::MessageList msgs = m_op != defaultOp() || !isClick
										? endSelection(contextId)
										: endDeselection(contextId);
			if(!msgs.isEmpty()) {
				msgs.prepend(net::makeUndoPointMessage(contextId));
				client->sendMessages(msgs.size(), msgs.constData());
			}
		}
		m_op = -1;
	}
}

void SelectionTool::finishMultipart()
{
	end();
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

bool SelectionTool::shouldDisguiseAsPutImage() const
{
	// If we're connected to a thick server, we don't want to send it unknown
	// message types because that'll get us kicked.
	return m_owner.client()->seemsConnectedToThickServer();
}

void SelectionTool::updateSelectionPreview(const QPainterPath &path) const
{
	emit m_owner.pathPreviewRequested(path);
}

void SelectionTool::removeSelectionPreview() const
{
	updateSelectionPreview(QPainterPath());
}

void SelectionTool::setOrRevertOp(int op)
{
	m_op = defaultOp() == op ? DP_MSG_SELECTION_PUT_OP_REPLACE : op;
}

net::MessageList SelectionTool::endDeselection(uint8_t contextId)
{
	cancelSelection();
	return {net::makeSelectionClearMessage(
		shouldDisguiseAsPutImage(), contextId,
		canvas::CanvasModel::MAIN_SELECTION_ID)};
}

bool SelectionTool::isInsideSelection(const QPointF &point) const
{
	canvas::CanvasModel *canvas = m_owner.model();
	if(canvas) {
		canvas::SelectionModel *selection = canvas->selection();
		if(selection->isValid()) {
			QPoint p = point.toPoint();
			const QRect &bounds = selection->bounds();
			return bounds.contains(p) &&
				   qAlpha(selection->mask().pixel(p - bounds.topLeft())) != 0;
		}
	}
	return false;
}


RectangleSelection::RectangleSelection(ToolController &owner)
	: SelectionTool(
		  owner, SELECTION,
		  QCursor(QPixmap(":cursors/select-rectangle.png"), 2, 2))
{
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
			shouldDisguiseAsPutImage(), contextId,
			canvas::CanvasModel::MAIN_SELECTION_ID)};
	} else {
		net::MessageList msgs;
		net::makeSelectionPutMessages(
			msgs, shouldDisguiseAsPutImage(), contextId,
			canvas::CanvasModel::MAIN_SELECTION_ID, op(), area.x(), area.y(),
			area.width(), area.height(), mask);
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
	: SelectionTool(
		  owner, POLYGONSELECTION,
		  QCursor(QPixmap(":cursors/select-lasso.png"), 2, 29))
{
}

void PolygonSelection::beginSelection(const canvas::Point &point)
{
	if(antiAlias()) {
		m_polygonF.clear();
		m_polygonF.append(point);
	} else {
		m_polygon.clear();
		m_polygon.append(point.toPoint());
	}
	updatePolygonSelectionPreview();
}

void PolygonSelection::continueSelection(const canvas::Point &point)
{
	if(antiAlias()) {
		if(!point.intSame(m_polygonF.last())) {
			m_polygonF.append(point);
			updatePolygonSelectionPreview();
		}
	} else {
		QPoint p = point.toPoint();
		if(p != m_polygon.last()) {
			m_polygon.append(p);
			updatePolygonSelectionPreview();
		}
	}
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
	removeSelectionPreview();
}

net::MessageList PolygonSelection::endSelection(uint8_t contextId)
{
	removeSelectionPreview();

	QRect area;
	if(antiAlias()) {
		if(m_polygonF.size() >= 2) {
			QRectF bounds = m_polygonF.boundingRect();
			area = bounds.toAlignedRect();
			m_polygonF.translate(-QPointF(area.topLeft()));
		}
	} else {
		if(m_polygon.size() >= 2) {
			area = m_polygon.boundingRect();
			m_polygon.translate(-area.topLeft());
		}
	}

	if(area.isEmpty()) {
		return {net::makeSelectionClearMessage(
			shouldDisguiseAsPutImage(), contextId,
			canvas::CanvasModel::MAIN_SELECTION_ID)};
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
			msgs, shouldDisguiseAsPutImage(), contextId,
			canvas::CanvasModel::MAIN_SELECTION_ID, op(), area.x(), area.y(),
			area.width(), area.height(), mask);
		return msgs;
	}
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
