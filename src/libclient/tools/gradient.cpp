// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/gradient.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/selectionmodel.h"
#include "libclient/tools/toolcontroller.h"
#include <QAtomicInteger>
#include <QCursor>
#include <QGradient>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>

namespace tools {

class GradientTool::Task final : public ToolController::Task {
public:
	Task(
		GradientTool *tool, QAtomicInteger<unsigned int> &currentExecutionId,
		const QPoint &pos, const QImage &mask, Type type, Spread spread,
		const QPointF &p1, const QPointF &p2, const QColor &c1,
		const QColor &c2, const qreal focus)
		: m_tool(tool)
		, m_executionId(++currentExecutionId)
		, m_currentExecutionId(currentExecutionId)
		, m_pos(pos)
		, m_type(type)
		, m_spread(spread)
		, m_line(p1 - pos, p2 - pos)
		, m_c1(c1)
		, m_c2(c2)
		, m_focus(focus)
		, m_img(mask)
	{
	}

	void run() override
	{
		if(m_img.isNull()) {
			m_error = QStringLiteral("No selection to apply the gradient on.");
		} else if(m_executionId != m_currentExecutionId) {
			m_img = QImage(); // Cancelled.
		} else {
			switch(m_type) {
			case Type::Linear:
				applyLinearGradient();
				break;
			case Type::Radial:
				applyRadialGradient();
				break;
			default:
				m_img = QImage();
				m_error = QStringLiteral("Invalid gradient type %1.")
							  .arg(int(m_type));
				break;
			}
		}
	}

	void finished() override { m_tool->gradientFinished(this); }

	unsigned int executionId() const { return m_executionId; }
	QImage &&takeImage() { return std::move(m_img); }
	const QPoint &pos() const { return m_pos; }
	const QString &error() const { return m_error; }

private:
	void applyLinearGradient()
	{
		QLinearGradient gradient(m_line.p1(), m_line.p2());
		prepareGradient(gradient);
		paintGradient(gradient);
	}

	void applyRadialGradient()
	{
		QRadialGradient gradient(
			m_line.p1(), m_line.length(),
			m_line.pointAt(qBound(0.0, m_focus, 1.0)));
		prepareGradient(gradient);
		paintGradient(gradient);
	}

	void prepareGradient(QGradient &gradient)
	{
		gradient.setColorAt(0.0, m_c1);
		gradient.setColorAt(1.0, m_c2);
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

	void paintGradient(QGradient &gradient)
	{
		QPainter painter(&m_img);
		painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
		painter.setPen(Qt::NoPen);
		painter.setBrush(gradient);
		painter.drawRect(m_img.rect());
	}

	GradientTool *m_tool;
	const unsigned int m_executionId;
	const QAtomicInteger<unsigned int> &m_currentExecutionId;
	const QPoint m_pos;
	const Type m_type;
	const Spread m_spread;
	const QLineF m_line;
	const QColor m_c1;
	const QColor m_c2;
	const qreal m_focus;
	QImage m_img;
	QString m_error;
};

GradientTool::GradientTool(ToolController &owner)
	: Tool(
		  owner, GRADIENT, QCursor(Qt::PointingHandCursor), true, false, false,
		  true, false)
	, m_blendMode(DP_BLEND_MODE_NORMAL)
{
}

void GradientTool::begin(const BeginParams &params)
{
	m_zoom = params.zoom;
	m_dragging = !params.right;
	if(m_dragging) {
		m_dragStartPoint = params.point;
		if(m_points.isEmpty()) {
			m_points = {params.point, params.point};
			m_dragIndex = 1;
		} else {
			updateHoverIndex(params.point);
			m_dragIndex = m_hoverIndex;
		}
		m_originalPoints = m_points;
		updateAnchorLine();
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
				m_points[m_dragIndex] = m_originalPoints[m_dragIndex] + delta;
			} else {
				// Can't happen, we currently only allow two points.
			}
		} else {
			for(int i = 0; i < pointCount; ++i) {
				m_points[i] = m_originalPoints[i] + delta;
			}
		}
		updateAnchorLine();
	} else if(hoverIndexChanged && m_dragIndex == -1) {
		emit m_owner.anchorLineActiveIndexRequested(m_hoverIndex);
	}
}

void GradientTool::hover(const HoverParams &params)
{
	m_zoom = params.zoom;
	if(updateHoverIndex(params.point) && m_dragIndex == -1) {
		emit m_owner.anchorLineActiveIndexRequested(m_hoverIndex);
	}
}

void GradientTool::end()
{
	canvas::CanvasModel *canvas = m_owner.model();
	if(canvas) {
		canvas::SelectionModel *selection = canvas->selection();
		if(selection->isValid()) {
			m_owner.executeAsync(new Task(
				this, m_currentExecutionId, selection->bounds().topLeft(),
				selection->mask(), m_type, m_spread, m_points.first(),
				m_points.last(), Qt::red, Qt::yellow, m_focus));
		}
	}

	if(m_dragging) {
		if(m_hoverIndex != m_dragIndex) {
			emit m_owner.anchorLineActiveIndexRequested(m_hoverIndex);
		}
		m_dragIndex = -1;
		pushPoints();
	}
}


void GradientTool::finishMultipart()
{
	cancelMultipart();
}

void GradientTool::cancelMultipart()
{
	if(isMultipart()) {
		++m_currentExecutionId;
		m_points.clear();
		m_pointsStack.clear();
		m_dragIndex = -1;
		m_hoverIndex = -1;
		m_pointsStackTop = -1;
		m_pendingImage = QImage();
		updateAnchorLine();
		previewPending();
	}
}

void GradientTool::undoMultipart()
{
	if(m_pointsStackTop > 0) {
		--m_pointsStackTop;
		m_points = m_pointsStack[m_pointsStackTop];
		updateAnchorLine();
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
	}
}

bool GradientTool::isMultipart() const
{
	return !m_points.isEmpty();
}

void GradientTool::gradientFinished(Task *task)
{
	unsigned int executionId = task->executionId();
	if(executionId == m_currentExecutionId) {
		m_finishedExecutionId = executionId;
		m_pendingImage = task->takeImage();
		if(m_pendingImage.isNull()) {
			qWarning("Gradient failed: %s", qUtf8Printable(task->error()));
		} else {
			m_pendingPos = task->pos();
		}
		previewPending();
	}
}

void GradientTool::pushPoints()
{
	if(m_pointsStack.isEmpty()) {
		m_pointsStack = {m_points};
		m_pointsStackTop = 0;
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
	if(m_zoom > 0.0) {
		qreal radius = (HANDLE_RADIUS + 2) / m_zoom;
		qreal bestDistance = -1.0;
		int pointCount = m_points.size();
		for(int i = 0; i < pointCount; ++i) {
			qreal distance = QLineF(targetPoint, m_points[i]).length();
			if(distance <= radius &&
			   (bestIndex < 0 || distance < bestDistance)) {
				bestIndex = i;
				bestDistance = distance;
			}
		}
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
