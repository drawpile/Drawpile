// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/shapetools.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/utils.h"
#include "libclient/utils/cursors.h"

namespace tools {

class PointVectorGenerator {
public:
	PointVectorGenerator() {}

	void append(const QPointF &point)
	{
		if(!m_pv.isEmpty()) {
			// MyPaint brushes don't take kindly to unnatural delta times. Very
			// short ones cause weird brush behavior and long ones (5 seconds+)
			// cause them to not draw the stroke at all. So we interpolate.
			QLineF line(m_pv.last(), point);
			int segments = qCeil(line.length() / SEGMENT_LENGTH);
			for(int i = 1; i < segments; ++i) {
				appendPoint(line.pointAt(1.0 / qreal(segments) * qreal(i)));
			}
		}
		appendPoint(point);
	}

	const canvas::PointVector &pv() { return m_pv; }

private:
	static constexpr qreal SEGMENT_LENGTH = 10.0;
	static constexpr qreal DTIME = 10.0;

	void appendPoint(const QPointF &point)
	{
		canvas::Point cp(m_time, point, 1.0);
		if(!m_pv.isEmpty()) {
			m_time += canvas::Point::distance(m_pv.last(), cp) * DTIME;
		}
		m_pv.append(cp);
	}

	long long m_time = 0LL;
	canvas::PointVector m_pv;
};

void ShapeTool::begin(const BeginParams &params)
{
	Q_ASSERT(!m_drawing);
	if(params.right) {
		return;
	}

	QPointF point = params.point;
	m_start = point;
	m_current = point;
	m_p1 = point;
	m_p2 = point;
	m_zoom = params.zoom;
	m_angle = params.angle;
	m_mirror = params.mirror;
	m_flip = params.flip;
	m_drawing = true;

	updatePreview();
}

void ShapeTool::motion(const MotionParams &params)
{
	if(m_drawing) {
		m_current = params.point;
		updateShape(params.constrain, params.center);
		updatePreview();
	}
}

void ShapeTool::modify(const ModifyParams &params)
{
	if(m_drawing) {
		updateShape(params.constrain, params.center);
		updatePreview();
	}
}

void ShapeTool::cancelMultipart()
{
	m_owner.model()->paintEngine()->clearDabsPreview();
	m_drawing = false;
}

void ShapeTool::setBrushSizeLimit(int limit)
{
	m_brushEngine.setSizeLimit(limit);
}

void ShapeTool::setSelectionMaskingEnabled(bool selectionMaskingEnabled)
{
	setCapability(Capability::IgnoresSelections, !selectionMaskingEnabled);
}

void ShapeTool::end(const EndParams &params)
{
	if(m_drawing) {
		updateShape(params.constrain, params.center);
		m_drawing = false;

		canvas::PaintEngine *paintEngine = m_owner.model()->paintEngine();
		drawdance::CanvasState canvasState = paintEngine->sampleCanvasState();

		m_owner.setBrushEngineBrush(m_brushEngine, false);

		const canvas::PointVector pv = pointVector();
		m_brushEngine.beginStroke(
			localUserId(), canvasState, isCompatibilityMode(), true, m_mirror,
			m_flip, m_zoom, m_angle);
		for(const canvas::Point &p : pv) {
			m_brushEngine.strokeTo(p, canvasState);
		}
		m_brushEngine.endStroke(pv.last().timeMsec() + 10, canvasState, true);

		paintEngine->clearDabsPreview();
		m_brushEngine.sendMessagesTo(m_owner.client());
	}
}

QPointF ShapeTool::getConstrainPoint() const
{
	return constraints::square(m_start, m_current);
}

void ShapeTool::updatePreview()
{
	m_owner.setBrushEngineBrush(m_brushEngine, false);
	canvas::PaintEngine *paintEngine = m_owner.model()->paintEngine();
	drawdance::CanvasState canvasState = paintEngine->sampleCanvasState();

	const canvas::PointVector pv = pointVector();
	Q_ASSERT(pv.count() > 1);
	m_brushEngine.beginStroke(
		localUserId(), canvasState, isCompatibilityMode(), false, m_mirror,
		m_flip, m_zoom, m_angle);
	for(const canvas::Point &p : pv) {
		m_brushEngine.strokeTo(p, canvasState);
	}
	m_brushEngine.endStroke(pv.last().timeMsec() + 10, canvasState, false);

	paintEngine->previewDabs(
		m_owner.activeLayerOrSelection(), m_brushEngine.messages());
	m_brushEngine.clearMessages();
}

void ShapeTool::updateShape(bool constrain, bool center)
{
	m_p2 = constrain ? getConstrainPoint() : m_current;
	m_p1 = center ? m_start - (m_p2 - m_start) : m_start;
}

Line::Line(ToolController &owner)
	: ShapeTool(owner, LINE, utils::Cursors::line())
{
}

canvas::PointVector Line::pointVector() const
{
	PointVectorGenerator gen;
	gen.append(m_p1);
	gen.append(m_p2);
	return gen.pv();
}

QPointF Line::getConstrainPoint() const
{
	return constraints::angle(m_start, m_current);
}

Rectangle::Rectangle(ToolController &owner)
	: ShapeTool(owner, RECTANGLE, utils::Cursors::rectangle())
{
}

canvas::PointVector Rectangle::pointVector() const
{
	PointVectorGenerator gen;
	gen.append(m_p1);
	gen.append(QPointF(m_p1.x(), m_p2.y()));
	gen.append(m_p2);
	gen.append(QPointF(m_p2.x(), m_p1.y()));
	gen.append(QPointF(m_p1.x(), m_p1.y()));
	return gen.pv();
}

Ellipse::Ellipse(ToolController &owner)
	: ShapeTool(owner, ELLIPSE, utils::Cursors::ellipse())
{
}

canvas::PointVector Ellipse::pointVector() const
{
	QRectF r = rect();
	qreal a = r.width() / 2.0;
	qreal b = r.height() / 2.0;
	qreal cx = r.x() + a;
	qreal cy = r.y() + b;
	PointVectorGenerator gen;

	qreal radius = (a + b) / 2.0;
	qreal step;
	if(radius > 2.0) {
		qreal t = 1.0 - 0.33 / radius;
		step = std::acos(2.0 * t * t - 1.0);
	} else {
		step = M_PI / 4.0;
	}

	for(qreal t = 0.0; t < 2.0 * M_PI; t += step) {
		gen.append(QPointF(cx + a * cos(t), cy + b * sin(t)));
	}
	gen.append(QPointF(cx + a, cy));

	return gen.pv();
}

}
