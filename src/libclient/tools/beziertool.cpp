/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017-2021 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"

#include "libclient/net/client.h"
#include "libclient/net/envelopebuilder.h"

#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/beziertool.h"

#include "rustpile/rustpile.h"

#include <QPixmap>

namespace tools {

using canvas::PointVector;
using canvas::Point;

BezierTool::BezierTool(ToolController &owner)
	: Tool(owner, BEZIER, QCursor(QPixmap(":cursors/curve.png"), 1, 1))
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

	if(!m_points.isEmpty())
		updatePreview();
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
}

void BezierTool::finishMultipart()
{
	if(m_points.size() > 2) {
		m_points.pop_back();

		auto brushengine = rustpile::brushengine_new();
		owner.setBrushEngineBrush(brushengine, false);

		const uint8_t contextId = owner.client()->myId();
		auto engine = owner.model()->paintEngine()->engine();

		const auto pv = calculateBezierCurve();
		for(const auto &p : pv) {
			rustpile::brushengine_stroke_to(brushengine, p.x(), p.y(), p.pressure(), 10, engine, owner.activeLayer());
		}
		rustpile::brushengine_end_stroke(brushengine);

		net::EnvelopeBuilder writer;
		rustpile::write_undopoint(writer, contextId);
		rustpile::brushengine_write_dabs(brushengine, contextId, writer);
		rustpile::write_penup(writer, contextId);

		rustpile::brushengine_free(brushengine);

		owner.client()->sendEnvelope(writer.toEnvelope());
	}
	cancelMultipart();
}

void BezierTool::cancelMultipart()
{
	m_points.clear();
	rustpile::paintengine_remove_preview(owner.model()->paintEngine()->engine(), owner.activeLayer());
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

	auto brushengine = rustpile::brushengine_new();
	owner.setBrushEngineBrush(brushengine, false);

	auto engine = owner.model()->paintEngine()->engine();

	for(const auto &p : pv) {
		rustpile::brushengine_stroke_to(brushengine, p.x(), p.y(), p.pressure(), 10, engine, owner.activeLayer());
	}
	rustpile::brushengine_end_stroke(brushengine);

	rustpile::paintengine_preview_brush(engine, owner.activeLayer(), brushengine);
	rustpile::brushengine_free(brushengine);
}

}

