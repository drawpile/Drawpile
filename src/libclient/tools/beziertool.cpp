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

#include "canvas/canvasmodel.h"
#include "canvas/paintengine.h"

#include "brushes/shapes.h"
#include "net/client.h"
#include "net/envelopebuilder.h"

#include "tools/toolcontroller.h"
#include "tools/beziertool.h"

#include "../rustpile/rustpile.h"

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
		rustpile::brushengine_set_classicbrush(brushengine, &owner.activeBrush().brush(), owner.activeLayer());

		const uint8_t contextId = owner.client()->myId();
		auto engine = owner.model()->paintEngine()->engine();

		const auto pv = calculateBezierCurve();
		for(const auto &p : pv) {
			rustpile::brushengine_stroke_to(brushengine, p.x(), p.y(), p.pressure(), engine, owner.activeLayer());
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

		pv << brushes::shapes::cubicBezierCurve(points);
	}

	return pv;
}


void BezierTool::updatePreview()
{
	const PointVector pv = calculateBezierCurve();
	if(pv.size()<=1)
		return;

	auto brushengine = rustpile::brushengine_new();
	rustpile::brushengine_set_classicbrush(brushengine, &owner.activeBrush().brush(), owner.activeLayer());

	auto engine = owner.model()->paintEngine()->engine();

	for(const auto &p : pv) {
		rustpile::brushengine_stroke_to(brushengine, p.x(), p.y(), p.pressure(), engine, owner.activeLayer());
	}
	rustpile::brushengine_end_stroke(brushengine);

	rustpile::paintengine_preview_brush(engine, owner.activeLayer(), brushengine);
	rustpile::brushengine_free(brushengine);
}

}

