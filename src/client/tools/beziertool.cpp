/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017-2019 Calle Laakkonen

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

#include "core/layerstack.h"
#include "core/layer.h"
#include "brushes/shapes.h"
#include "brushes/brushengine.h"
#include "brushes/brushpainter.h"
#include "net/client.h"
#include "net/commands.h"

#include "tools/toolcontroller.h"
#include "tools/beziertool.h"

#include "../shared/net/undo.h"

#include <QPixmap>

namespace tools {

using paintcore::PointVector;
using paintcore::Point;

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

		const paintcore::Layer *layer = owner.model()->layerStack()->getLayer(owner.activeLayer());

		brushes::BrushEngine brushengine;
		brushengine.setBrush(owner.client()->myId(), owner.activeLayer(), owner.activeBrush());

		const auto pv = calculateBezierCurve();
		for(const Point &p : pv)
			brushengine.strokeTo(p, layer);
		brushengine.endStroke();

		const uint8_t contextId = owner.client()->myId();
		QList<protocol::MessagePtr> msgs;
		msgs << protocol::MessagePtr(new protocol::UndoPoint(contextId));
		msgs << brushengine.takeDabs();
		msgs << protocol::MessagePtr(new protocol::PenUp(contextId));
		owner.client()->sendMessages(msgs);
	}

	cancelMultipart();
}

void BezierTool::cancelMultipart()
{
	m_points.clear();
	auto layers = owner.model()->layerStack()->editor();
	auto layer = layers.getEditableLayer(owner.activeLayer());
	if(!layer.isNull())
		layer.removeSublayer(-1);
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
	auto layers = owner.model()->layerStack()->editor();
	auto layer = layers.getEditableLayer(owner.activeLayer());
	if(layer.isNull()) {
		qWarning("BezierTool::updatePreview: no active layer!");
		return;
	}

	const PointVector pv = calculateBezierCurve();
	if(pv.size()<=1)
		return;

	brushes::BrushEngine brushengine;
	brushengine.setBrush(0, 0, owner.activeBrush());

	for(int i=0;i<pv.size();++i)
		brushengine.strokeTo(pv.at(i), layer.layer());
	brushengine.endStroke();

	layer.removeSublayer(-1);

	const auto dabs = brushengine.takeDabs();
	for(int i=0;i<dabs.size();++i)
		brushes::drawBrushDabsDirect(*dabs.at(i), layer, -1);
}

}

