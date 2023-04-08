// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"

#include "libclient/net/client.h"

#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/beziertool.h"

#include <QPixmap>

namespace tools {

using canvas::PointVector;
using canvas::Point;

BezierTool::BezierTool(ToolController &owner)
	: Tool(owner, BEZIER, QCursor(QPixmap(":cursors/curve.png"), 1, 1), true, true, false)
	, m_brushEngine{}
{
}

void BezierTool::begin(const Point& point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	m_rightButton = right;

	if(right) {
		if(m_points.size()>2) {
			m_points.pop_back();
			m_points.last().point = point;
			m_beginPoint = point;

		} else {
			cancelMultipart();
		}

	} else {
		if(m_points.isEmpty()) {
			m_points << ControlPoint { point, QPointF() };
		}

		m_beginPoint = point;
	}

	bool hasPoints = !m_points.isEmpty();
	setHandlesRightClick(hasPoints);
	if(hasPoints) {
		updatePreview();
	}
}

void BezierTool::motion(const Point& point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);
	if(m_rightButton)
		return;

	if(m_points.isEmpty()) {
		qWarning("BezierTool::motion: point vector is empty!");
		return;
	}

	m_points.last().cp = m_beginPoint - point;
	updatePreview();
}

void BezierTool::hover(const QPointF &point)
{
	if(m_points.isEmpty())
		return;

	if(!Point::intSame(point, m_points.last().point)) {
		m_points.last().point = point;
		updatePreview();
	}
}

void BezierTool::end()
{
	const int s = m_points.size();

	if(m_rightButton || s==0)
		return;

	m_points << ControlPoint { m_points.last().point, QPointF() };
	if(s > 1 && Point::intSame(m_points.at(s-1).cp, QPointF()))
		finishMultipart();

	setHandlesRightClick(!m_points.isEmpty());
}

void BezierTool::finishMultipart()
{
	if(m_points.size() > 2) {
		m_points.pop_back();

		m_owner.setBrushEngineBrush(m_brushEngine, false);

		net::Client *client = m_owner.client();
		const uint8_t contextId = client->myId();
		drawdance::CanvasState canvasState = m_owner.model()->paintEngine()->sampleCanvasState();

		const PointVector pv = calculateBezierCurve();
		m_brushEngine.beginStroke(contextId);
		for(const canvas::Point &p : pv) {
			m_brushEngine.strokeTo(p.x(), p.y(), p.pressure(), p.xtilt(), p.ytilt(), p.rotation(), 10, canvasState);
		}
		m_brushEngine.endStroke();

		m_brushEngine.sendMessagesTo(client);
	}
	cancelMultipart();
}

void BezierTool::cancelMultipart()
{
	m_points.clear();
	m_owner.model()->paintEngine()->clearPreview();
}

void BezierTool::undoMultipart()
{
	if(!m_points.isEmpty()) {
		m_points.pop_back();
		if(m_points.size() <= 1)
			cancelMultipart();
		else
			updatePreview();
	}
}

static Point _cubicBezierPoint(const QPointF p[4], float t)
{
	const float t1 = 1-t;
	const float Ax = t1*p[0].x() + t*p[1].x();
	const float Ay = t1*p[0].y() + t*p[1].y();
	const float Bx = t1*p[1].x() + t*p[2].x();
	const float By = t1*p[1].y() + t*p[2].y();
	const float Cx = t1*p[2].x() + t*p[3].x();
	const float Cy = t1*p[2].y() + t*p[3].y();

	const float Dx = t1*Ax + t*Bx;
	const float Dy = t1*Ay + t*By;
	const float Ex = t1*Bx + t*Cx;
	const float Ey = t1*By + t*Cy;

	return Point(t1*Dx + t*Ex, t1*Dy + t*Ey, 1);;
}

PointVector BezierTool::calculateBezierCurve() const
{
	PointVector pv;
	if(m_points.isEmpty())
		return pv;
	if(m_points.size()==1) {
		pv << Point(m_points.first().point, 1);
		return pv;
	}

	for(int i=1;i<m_points.size();++i) {
		const QPointF points[4] = {
			m_points[i-1].point,
			m_points[i-1].point - m_points[i-1].cp,
			m_points[i].point + m_points[i].cp,
			m_points[i].point
		};

		// TODO smart step size selection
		for(float t=0;t<1;t+=0.05f) {
			pv << _cubicBezierPoint(points, t);
		}
	}

	return pv;
}


void BezierTool::updatePreview()
{
	const PointVector pv = calculateBezierCurve();
	if(pv.size()<=1)
		return;

	m_owner.setBrushEngineBrush(m_brushEngine, false);

	canvas::PaintEngine *paintEngine = m_owner.model()->paintEngine();
	drawdance::CanvasState canvasState = paintEngine->sampleCanvasState();
	m_brushEngine.beginStroke(0);
	for(const canvas::Point &p : pv) {
		m_brushEngine.strokeTo(p.x(), p.y(), p.pressure(), p.xtilt(), p.ytilt(), p.rotation(), 10, canvasState);
	}
	m_brushEngine.endStroke();

	paintEngine->previewDabs(m_owner.activeLayer(), m_brushEngine.messages());
	m_brushEngine.clearMessages();
}

}

