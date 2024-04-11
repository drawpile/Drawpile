// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"

#include "libclient/net/client.h"

#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/shapetools.h"
#include "libclient/tools/utils.h"

#include <QPixmap>

namespace tools {

class PointVectorGenerator {
public:
	PointVectorGenerator()
		: m_time{0}
		, m_pv{}
	{
	}

	void append(const QPointF &point)
	{
		if(!m_pv.isEmpty()) {
			// MyPaint brushes don't take kindly to unnatural delta times. Very
			// short ones cause weird brush behavior and long ones (5 seconds+)
			// cause them to not draw the stroke at all. So we interpolate.
			QLineF line{m_pv.last(), point};
			int segments = qCeil(line.length() / SEGMENT_LENGTH);
			for(int i = 1; i < segments; ++i) {
				appendPoint(line.pointAt(1.0 / qreal(segments) * qreal(i)));
			}
		}
		appendPoint(point);
	}

	const canvas::PointVector &pv()
	{
		return m_pv;
	}

private:
	static constexpr qreal SEGMENT_LENGTH = 10.0;
	static constexpr qreal DTIME = 10.0;

	void appendPoint(const QPointF &point)
	{
		canvas::Point cp{m_time, point, 1.0};
		if(!m_pv.isEmpty()) {
			m_time += canvas::Point::distance(m_pv.last(), cp) * DTIME;
		}
		m_pv.append(cp);
	}

	long long m_time;
	canvas::PointVector m_pv;
};

void ShapeTool::begin(
	const canvas::Point &point, bool right, float zoom, const QPointF &viewPos)
{
	Q_UNUSED(viewPos);
	Q_ASSERT(!m_drawing);
	if(right) {
		return;
	}

	m_start = point;
	m_zoom = zoom;
	m_p1 = point;
	m_p2 = point;
	m_drawing = true;

	updatePreview();
}

void ShapeTool::motion(
	const canvas::Point &point, bool constrain, bool center,
	const QPointF &viewPos)
{
	Q_UNUSED(viewPos);
	if(!m_drawing)
		return;

	if(constrain)
		m_p2 = constraints::square(m_start, point);
	else
		m_p2 = point;

	if(center)
		m_p1 = m_start - (m_p2 - m_start);
	else
		m_p1 = m_start;

	updatePreview();
}

void ShapeTool::cancelMultipart()
{
	m_owner.model()->paintEngine()->clearDabsPreview();
	m_drawing = false;
}

void ShapeTool::end()
{
	if(!m_drawing)
		return;

	m_drawing = false;

	net::Client *client = m_owner.client();
	canvas::PaintEngine *paintEngine = m_owner.model()->paintEngine();
	drawdance::CanvasState canvasState = paintEngine->sampleCanvasState();

	m_owner.setBrushEngineBrush(m_brushEngine, false);

	const canvas::PointVector pv = pointVector();
	m_brushEngine.beginStroke(client->myId(), true, m_zoom);
	for(const canvas::Point &p : pv) {
		m_brushEngine.strokeTo(p, canvasState);
	}
	m_brushEngine.endStroke(pv.last().timeMsec() + 10, canvasState, true);

	paintEngine->clearDabsPreview();
	m_brushEngine.sendMessagesTo(client);
}

void ShapeTool::updatePreview()
{
	m_owner.setBrushEngineBrush(m_brushEngine, false);
	canvas::PaintEngine *paintEngine = m_owner.model()->paintEngine();
	drawdance::CanvasState canvasState = paintEngine->sampleCanvasState();

	const canvas::PointVector pv = pointVector();
	Q_ASSERT(pv.count() > 1);
	m_brushEngine.beginStroke(0, false, m_zoom);
	for(const canvas::Point &p : pv) {
		m_brushEngine.strokeTo(p, canvasState);
	}
	m_brushEngine.endStroke(pv.last().timeMsec() + 10, canvasState, false);

	paintEngine->previewDabs(m_owner.activeLayer(), m_brushEngine.messages());
	m_brushEngine.clearMessages();
}

Line::Line(ToolController &owner)
	: ShapeTool(owner, LINE, QCursor(QPixmap(":cursors/line.png"), 2, 2))
{
}

void Line::motion(
	const canvas::Point &point, bool constrain, bool center,
	const QPointF &viewPos)
{
	Q_UNUSED(viewPos);
	if(!m_drawing)
		return;

	if(constrain)
		m_p2 = constraints::angle(m_start, point);
	else
		m_p2 = point;

	if(center)
		m_p1 = m_start - (m_p2 - m_start);
	else
		m_p1 = m_start;

	updatePreview();
}

canvas::PointVector Line::pointVector() const
{
	PointVectorGenerator gen;
	gen.append(m_p1);
	gen.append(m_p2);
	return gen.pv();
}

Rectangle::Rectangle(ToolController &owner)
	: ShapeTool(owner, RECTANGLE, QCursor(QPixmap(":cursors/rectangle.png"), 2, 2))
{
}

canvas::PointVector Rectangle::pointVector() const
{
	PointVectorGenerator gen;
	gen.append(m_p1);
	gen.append(QPointF{m_p1.x(), m_p2.y()});
	gen.append(m_p2);
	gen.append(QPointF{m_p2.x(), m_p1.y()});
	gen.append(QPointF{m_p1.x(), m_p1.y()});
	return gen.pv();
}

Ellipse::Ellipse(ToolController &owner)
	: ShapeTool(owner, ELLIPSE, QCursor(QPixmap(":cursors/ellipse.png"), 2, 2))
{
}

canvas::PointVector Ellipse::pointVector() const
{
	const auto r = rect();
	const qreal a = r.width() / 2.0;
	const qreal b = r.height() / 2.0;
	const qreal cx = r.x() + a;
	const qreal cy = r.y() + b;
	PointVectorGenerator gen;

	qreal radius = (a + b) / 2.0;
	qreal step;
	if (radius > 2.0) {
		qreal t = 1.0 - 0.33 / radius;
		step = std::acos(2.0 * t * t - 1.0);
	} else {
		step = M_PI / 4.0;
	}

	for(qreal t = 0.0; t < 2.0 * M_PI; t += step) {
		gen.append(QPointF{cx + a*cos(t), cy + b*sin(t)});
	}
	gen.append(QPointF{cx+a, cy});

	return gen.pv();
}

}
