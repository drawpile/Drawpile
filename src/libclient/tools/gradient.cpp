// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/gradient.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/selectionmodel.h"
#include "libclient/net/client.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/utils.h"
#include "libclient/utils/cursors.h"
#include <QAtomicInteger>
#include <QCursor>
#include <QGradient>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>

namespace tools {

GradientTool::GradientTool(ToolController &owner)
	: Tool(
		  owner, GRADIENT, utils::Cursors::gradient(),
		  Capability::AllowColorPick)
	, m_blendMode(DP_BLEND_MODE_NORMAL)
	, m_previewDebounce(200)
{
	QObject::connect(
		&m_previewDebounce, &DebounceTimer::noneChanged, &owner, [this] {
			updatePending();
		});
}

void GradientTool::begin(const BeginParams &params)
{
	m_zoom = params.zoom;
	if(params.right) {
		cancelMultipart();
	} else {
		m_dragStartPoint = params.point;
		if(m_points.isEmpty()) {
			m_points = {params.point, params.point};
			m_dragIndex = 1;
			m_dragging = true;
			m_originalPoints = m_points;
			m_clickDetector.begin(params.viewPos, params.deviceType);
			updateAnchorLine();
			updateCursor();
		} else {
			updateHoverIndex(params.point);
			if(m_hoverOutside) {
				finishMultipart();
			} else {
				m_dragIndex = m_hoverIndex;
				m_dragging = true;
				m_originalPoints = m_points;
				m_clickDetector.begin(params.viewPos, params.deviceType);
				updateAnchorLine();
				updateCursor();
			}
		}
	}
}

void GradientTool::motion(const MotionParams &params)
{
	bool hoverIndexChanged = updateHoverIndex(params.point);
	if(m_dragging) {
		int pointCount = m_points.size();
		QPointF delta = params.point - m_dragStartPoint;
		if(m_dragIndex >= 0 && m_dragIndex < pointCount) {
			if(m_dragIndex == 0 || m_dragIndex == pointCount - 1) {
				QPointF &point = m_points[m_dragIndex];
				const QPointF &originalPoint = m_originalPoints[m_dragIndex];
				if(params.constrain) {
					const QPointF &otherPoint = m_dragIndex == 0
													? m_points.constLast()
													: m_points.constFirst();
					point =
						constraints::angle(otherPoint, originalPoint + delta);
				} else {
					point = originalPoint + delta;
				}
			} else {
				// Can't happen, we currently only allow two points.
			}
		} else {
			if(params.constrain) {
				if(qAbs(delta.x()) < qAbs(delta.y())) {
					delta.setX(0.0);
				} else {
					delta.setY(0.0);
				}
			}
			for(int i = 0; i < pointCount; ++i) {
				m_points[i] = m_originalPoints[i] + delta;
			}
		}
		updateAnchorLine();
		m_previewDebounce.setNone();
	} else {
		if(hoverIndexChanged && m_dragIndex == -1) {
			emit m_owner.anchorLineActiveIndexRequested(m_hoverIndex);
		}
		updateCursor();
	}
}

void GradientTool::hover(const HoverParams &params)
{
	m_zoom = params.zoom;
	if(isMultipart() && updateHoverIndex(params.point) && m_dragIndex == -1) {
		emit m_owner.anchorLineActiveIndexRequested(m_hoverIndex);
	}
	updateCursor();
}

void GradientTool::end(const EndParams &params)
{
	Q_UNUSED(params);
	if(m_previewDebounce.stopTimer()) {
		updatePending();
	}

	if(m_dragging) {
		if(m_hoverIndex != m_dragIndex) {
			emit m_owner.anchorLineActiveIndexRequested(m_hoverIndex);
		}
		m_dragging = false;
		m_dragIndex = -1;
		m_clickDetector.end();
		if(m_pointsStack.isEmpty() && m_clickDetector.isClick()) {
			cancelMultipart();
			emit m_owner.showMessageRequested(QCoreApplication::translate(
				"tools::GradientSettings",
				//: This message is shown when the user misuses the gradient
				//: tool by just clicking in a single spot instead of dragging.
				"You need to drag to create a gradient."));
		} else {
			pushPoints();
		}
	}

	updateCursor();
}


void GradientTool::finishMultipart()
{
	if(isMultipart()) {
		int layerId = m_owner.activeLayer();
		canvas::CanvasModel *canvas = m_owner.model();
		bool canFill = !m_pendingImage.isNull() && layerId > 0 && canvas &&
					   !canvas->layerlist()
							->layerIndex(layerId)
							.data(canvas::LayerListModel::IsGroupRole)
							.toBool();
		if(canFill) {
			uint8_t contextId = localUserId();
			net::MessageList msgs;
			net::makePutImageMessagesCompat(
				msgs, contextId, m_owner.activeLayer(), m_blendMode,
				m_pendingPos.x(), m_pendingPos.y(), m_pendingImage,
				isCompatibilityMode());
			if(!msgs.isEmpty()) {
				msgs.prepend(net::makeUndoPointMessage(contextId));
				m_owner.client()->sendMessages(msgs.size(), msgs.constData());
			}
		}
		cancelMultipart();
	}
}

void GradientTool::cancelMultipart()
{
	if(isMultipart()) {
		m_points.clear();
		m_pointsStack.clear();
		m_dragIndex = -1;
		m_hoverIndex = -1;
		m_pointsStackTop = -1;
		m_pendingImage = QImage();
		m_clickDetector.clear();
		updateAnchorLine();
		previewPending();
		updateCursor();
		emit m_owner.gradientStateChanged(false);
	}
}

void GradientTool::undoMultipart()
{
	if(m_pointsStackTop > 0) {
		--m_pointsStackTop;
		m_points = m_pointsStack[m_pointsStackTop];
		updateAnchorLine();
		updateCursor();
	} else {
		cancelMultipart();
	}
}

void GradientTool::redoMultipart()
{
	if(m_pointsStackTop + 1 < m_pointsStack.size()) {
		++m_pointsStackTop;
		m_points = m_pointsStack[m_pointsStackTop];
		updateAnchorLine();
		updateCursor();
	}
}

bool GradientTool::isMultipart() const
{
	return !m_points.isEmpty();
}

void GradientTool::setSelectionValid(bool selectionValid)
{
	if(isMultipart()) {
		if(selectionValid) {
			updatePending();
		} else {
			finishMultipart();
		}
	}
}

void GradientTool::setParameters(
	const QColor &color1, const QColor &color2, Shape shape, Spread spread,
	qreal focus, int blendMode)
{
	if(color1 != m_color1 || color2 != m_color2 || shape != m_shape ||
	   spread != m_spread || focus != m_focus || blendMode != m_blendMode) {
		m_color1 = color1;
		m_color2 = color2;
		m_shape = shape;
		m_spread = spread;
		m_focus = focus;
		m_blendMode = blendMode;
		updatePending();
	}
}

void GradientTool::pushPoints()
{
	if(m_pointsStack.isEmpty()) {
		m_pointsStack = {m_points};
		m_pointsStackTop = 0;
		emit m_owner.gradientStateChanged(true);
	} else if(m_pointsStack[m_pointsStackTop] != m_points) {
		int popCount = m_pointsStack.size() - m_pointsStackTop - 1;
		if(popCount > 0) {
			m_pointsStack.remove(m_pointsStackTop + 1, popCount);
		}

		int shiftCount = (m_pointsStack.size() + 1) - MAX_POINTS_STACK_DEPTH;
		if(shiftCount > 0) {
			m_pointsStack.remove(0, shiftCount);
		}

		m_pointsStack.append(m_points);
		m_pointsStackTop = m_pointsStack.size() - 1;
	}
}

bool GradientTool::updateHoverIndex(const QPointF &targetPoint)
{
	int bestIndex = -1;
	int pointCount = m_points.size();
	qreal radius = (HANDLE_RADIUS + 2) / m_zoom;
	if(m_zoom > 0.0 && pointCount != 0) {
		qreal bestDistance = -1.0;
		for(int i = 0; i < pointCount; ++i) {
			qreal distance = QLineF(targetPoint, m_points[i]).length();
			if(distance <= radius &&
			   (bestIndex < 0 || distance < bestDistance)) {
				bestIndex = i;
				bestDistance = distance;
			}
		}
	}

	if(bestIndex == -1 && pointCount != 0) {
		QLineF axis(m_points.constFirst(), m_points.constLast());
		QLineF cross =
			QLineF::fromPolar(1.0, axis.angle() + 90.0).translated(targetPoint);
		QPointF pointOnAxis;
		m_hoverOutside =
			axis.intersects(cross, &pointOnAxis) == QLineF::NoIntersection ||
			QLineF(targetPoint, pointOnAxis).length() > radius ||
			QLineF::fromPolar(radius, QLineF(targetPoint, pointOnAxis).angle())
					.translated(targetPoint)
					.intersects(axis, nullptr) != QLineF::BoundedIntersection;
	} else {
		m_hoverOutside = false;
	}

	if(bestIndex != m_hoverIndex) {
		m_hoverIndex = bestIndex;
		return true;
	} else {
		return false;
	}
}

void GradientTool::updateAnchorLine()
{
	emit m_owner.anchorLineRequested(m_points, m_dragIndex);
}

void GradientTool::updateCursor()
{
	if(isMultipart()) {
		if(m_dragging) {
			setCursor(Qt::ClosedHandCursor);
		} else if(m_hoverIndex == -1) {
			if(m_hoverOutside) {
				setCursor(utils::Cursors::check());
			} else {
				setCursor(utils::Cursors::move());
			}
		} else {
			setCursor(Qt::OpenHandCursor);
		}
	} else {
		setCursor(utils::Cursors::gradient());
	}
}

void GradientTool::updatePending()
{
	m_previewDebounce.stopTimer();
	QPoint pos;
	QImage img;

	if(isMultipart()) {
		canvas::CanvasModel *canvas = m_owner.model();
		if(canvas) {
			canvas::SelectionModel *selection = canvas->selection();
			if(selection->isValid()) {
				pos = selection->bounds().topLeft();
				img = applyGradient(
					selection->image(),
					QLineF(m_points.first() - pos, m_points.last() - pos));
			}
		}
	}

	if(!img.isNull() || !m_pendingImage.isNull()) {
		m_pendingPos = pos;
		m_pendingImage = img;
		previewPending();
	}
}

QImage GradientTool::applyGradient(const QImage &mask, const QLineF &line) const
{
	if(mask.isNull()) {
		return QImage();
	} else {
		switch(m_shape) {
		case Shape::Linear:
			return applyLinearGradient(mask, line);
		case Shape::Radial:
			return applyRadialGradient(mask, line);
		default:
			qWarning("Invalid gradient shape %d", int(m_shape));
			return QImage();
		}
	}
}

QImage
GradientTool::applyLinearGradient(const QImage &mask, const QLineF &line) const
{
	QLinearGradient gradient(line.p1(), line.p2());
	prepareGradient(gradient);
	return paintGradient(mask, gradient);
}

QImage
GradientTool::applyRadialGradient(const QImage &mask, const QLineF &line) const
{
	QRadialGradient gradient(
		line.p1(), line.length(), line.pointAt(qBound(0.0, m_focus, 1.0)));
	prepareGradient(gradient);
	return paintGradient(mask, gradient);
}

void GradientTool::prepareGradient(QGradient &gradient) const
{
	gradient.setColorAt(0.0, m_color1);
	gradient.setColorAt(1.0, m_color2);
	switch(m_spread) {
	case Spread::Pad:
		gradient.setSpread(QGradient::PadSpread);
		break;
	case Spread::Reflect:
		gradient.setSpread(QGradient::ReflectSpread);
		break;
	case Spread::Repeat:
		gradient.setSpread(QGradient::RepeatSpread);
		break;
	default:
		qWarning("Unknown gradient spread %d", int(m_spread));
		break;
	}
}

QImage
GradientTool::paintGradient(const QImage &mask, QGradient &gradient) const
{
	QImage img = mask.copy();
	QPainter painter(&img);
	painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
	painter.setPen(Qt::NoPen);
	painter.setBrush(gradient);
	painter.drawRect(img.rect());
	return img;
}

void GradientTool::previewPending()
{
	canvas::CanvasModel *canvas = m_owner.model();
	if(canvas) {
		canvas::PaintEngine *pe = canvas->paintEngine();
		if(m_pendingImage.isNull()) {
			pe->clearFillPreview();
		} else {
			pe->previewFill(
				m_owner.activeLayer(), m_blendMode, 1.0, m_pendingPos.x(),
				m_pendingPos.y(), m_pendingImage);
		}
	}
}

}
