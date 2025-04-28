// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/beziertool.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/net/client.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/utils/cursors.h"
#include <QLineF>

namespace tools {

using canvas::Point;
using canvas::PointVector;

static constexpr long long DELTA_MSEC = 10;

BezierTool::BezierTool(ToolController &owner)
	: Tool(
		  owner, BEZIER, utils::Cursors::curve(),
		  Capability::AllowColorPick | Capability::AllowToolAdjust |
			  Capability::Fractional)
{
}

void BezierTool::begin(const BeginParams &params)
{
	m_rightButton = params.right;
	m_zoom = params.zoom;
	m_angle = params.angle;
	m_mirror = params.mirror;
	m_flip = params.flip;

	QPointF point = params.point;
	if(m_rightButton) {
		if(m_points.size() > 2) {
			m_points.pop_back();
			m_points.last() = {point, QPointF()};
			m_beginPoint = point;

		} else {
			cancelMultipart();
		}

	} else {
		if(m_points.isEmpty()) {
			m_points << ControlPoint{point, QPointF()};
		}

		m_beginPoint = point;
	}

	bool hasPoints = !m_points.isEmpty();
	setCapability(Capability::HandlesRightClick, hasPoints);
	if(hasPoints) {
		updatePreview();
	}
}

void BezierTool::motion(const MotionParams &params)
{
	if(m_rightButton)
		return;

	if(m_points.isEmpty()) {
		qWarning("BezierTool::motion: point vector is empty!");
		return;
	}

	m_points.last().cp = m_beginPoint - params.point;
	updatePreview();
}

void BezierTool::hover(const HoverParams &params)
{
	if(m_points.isEmpty())
		return;

	QPointF point = params.point;
	if(!Point::intSame(point, m_points.last().point)) {
		m_points.last().point = point;
		updatePreview();
	}
}

void BezierTool::end(const EndParams &)
{
	int s = m_points.size();

	if(m_rightButton || s == 0) {
		return;
	}

	m_points << ControlPoint{m_points.last().point, QPointF()};
	if(s > 1 && Point::intSame(m_points.at(s - 1).cp, QPointF())) {
		finishMultipart();
	}

	setCapability(Capability::HandlesRightClick, !m_points.isEmpty());
}

void BezierTool::finishMultipart()
{
	if(m_points.size() > 2) {
		m_points.pop_back();

		m_owner.setBrushEngineBrush(m_brushEngine, false);

		net::Client *client = m_owner.client();
		const uint8_t contextId = client->myId();
		drawdance::CanvasState canvasState =
			m_owner.model()->paintEngine()->sampleCanvasState();

		const PointVector pv = calculateBezierCurve();
		m_brushEngine.beginStroke(
			contextId, canvasState, true, m_mirror, m_flip, m_zoom, m_angle);
		for(const canvas::Point &p : pv) {
			m_brushEngine.strokeTo(p, canvasState);
		}
		m_brushEngine.endStroke(
			pv.last().timeMsec() + DELTA_MSEC, canvasState, true);

		m_brushEngine.sendMessagesTo(client);
	}
	cancelMultipart();
}

void BezierTool::cancelMultipart()
{
	m_points.clear();
	m_owner.model()->paintEngine()->clearDabsPreview();
}

void BezierTool::undoMultipart()
{
	if(!m_points.isEmpty()) {
		m_points.pop_back();
		if(m_points.size() <= 1) {
			cancelMultipart();
		} else {
			m_points.last().cp = QPointF();
			updatePreview();
		}
	}
}

void BezierTool::setBrushSizeLimit(int limit)
{
	m_brushEngine.setSizeLimit(limit);
}

PointVector BezierTool::calculateBezierCurve() const
{
	long long timeMsec = 0;

	PointVector pv;
	if(m_points.isEmpty()) {
		return pv;
	} else if(m_points.size() == 1) {
		pv << Point(timeMsec, m_points.first().point, 1);
		return pv;
	}

	for(int i = 1; i < m_points.size(); ++i) {
		const QPointF points[4] = {
			m_points[i - 1].point, m_points[i - 1].point - m_points[i - 1].cp,
			m_points[i].point + m_points[i].cp, m_points[i].point};

		qreal distance = cubicBezierDistance(points);
		float stepSize = distance > 0.0 ? float(5.0 / distance) : 1.0f;
		for(float t = 0.0f; t < 1.0f; t += stepSize) {
			pv.append(Point(timeMsec, cubicBezierPoint(points, t), 1));
			timeMsec += DELTA_MSEC;
		}
	}

	return pv;
}

qreal BezierTool::cubicBezierDistance(const QPointF points[4])
{
	// Guess the length of the curve by drawing it with a low resolution.
	constexpr float STEP = 0.1f;
	QPointF prev = cubicBezierPoint(points, 0.0f);
	qreal distance = 0.0;
	for(float t = STEP; t < 1.0f; t += STEP) {
		QPointF p = cubicBezierPoint(points, t);
		distance += QLineF(prev, p).length();
		prev = p;
	}
	QPointF last = cubicBezierPoint(points, 1.0f);
	distance += QLineF(prev, last).length();
	return distance;
}

QPointF BezierTool::cubicBezierPoint(const QPointF points[4], float t)
{
	const float t1 = 1 - t;
	const float Ax = t1 * points[0].x() + t * points[1].x();
	const float Ay = t1 * points[0].y() + t * points[1].y();
	const float Bx = t1 * points[1].x() + t * points[2].x();
	const float By = t1 * points[1].y() + t * points[2].y();
	const float Cx = t1 * points[2].x() + t * points[3].x();
	const float Cy = t1 * points[2].y() + t * points[3].y();

	const float Dx = t1 * Ax + t * Bx;
	const float Dy = t1 * Ay + t * By;
	const float Ex = t1 * Bx + t * Cx;
	const float Ey = t1 * By + t * Cy;

	return QPointF(t1 * Dx + t * Ex, t1 * Dy + t * Ey);
}

void BezierTool::updatePreview()
{
	const PointVector pv = calculateBezierCurve();
	if(pv.size() <= 1)
		return;

	m_owner.setBrushEngineBrush(m_brushEngine, false);

	canvas::PaintEngine *paintEngine = m_owner.model()->paintEngine();
	drawdance::CanvasState canvasState = paintEngine->sampleCanvasState();
	m_brushEngine.beginStroke(
		m_owner.client()->myId(), canvasState, false, m_mirror, m_flip, m_zoom,
		m_angle);
	for(const canvas::Point &p : pv) {
		m_brushEngine.strokeTo(p, canvasState);
	}
	m_brushEngine.endStroke(
		pv.last().timeMsec() + DELTA_MSEC, canvasState, false);

	paintEngine->previewDabs(
		m_owner.activeLayerOrSelection(), m_brushEngine.messages());
	m_brushEngine.clearMessages();
}

}
