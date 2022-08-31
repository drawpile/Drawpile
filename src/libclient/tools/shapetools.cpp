/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2021 Calle Laakkonen

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

#include "net/client.h"

#include "tools/toolcontroller.h"
#include "tools/shapetools.h"
#include "tools/utils.h"

#include <QPixmap>

namespace tools {

void ShapeTool::begin(const canvas::Point& point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	Q_UNUSED(right);
	Q_ASSERT(!m_drawing);

	m_start = point;
	m_p1 = point;
	m_p2 = point;
	m_drawing = true;

	updatePreview();
}

void ShapeTool::motion(const canvas::Point& point, bool constrain, bool center)
{
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
	owner.model()->paintEngine()->clearPreview();
	m_drawing = false;
}

void ShapeTool::end()
{
	if(!m_drawing)
		return;

	m_drawing = false;

	net::Client *client = owner.client();
	canvas::PaintEngine *paintEngine = owner.model()->paintEngine();
	drawdance::CanvasState canvasState = paintEngine->viewCanvasState();

	owner.setBrushEngineBrush(m_brushEngine, false);

	const canvas::PointVector pv = pointVector();
	m_brushEngine.beginStroke(client->myId());
	for(const canvas::Point &p : pv) {
		m_brushEngine.strokeTo(p.x(), p.y(), p.pressure(), p.xtilt(), p.ytilt(), p.rotation(), 10, canvasState);
	}
	m_brushEngine.endStroke();

	paintEngine->clearPreview();
	m_brushEngine.sendMessagesTo(client);
}

void ShapeTool::updatePreview()
{
	owner.setBrushEngineBrush(m_brushEngine, false);
	canvas::PaintEngine *paintEngine = owner.model()->paintEngine();
	drawdance::CanvasState canvasState = paintEngine->viewCanvasState();

	const canvas::PointVector pv = pointVector();
	Q_ASSERT(pv.count() > 1);
	m_brushEngine.beginStroke(0);
	for(const canvas::Point &p : pv) {
		m_brushEngine.strokeTo(p.x(), p.y(), p.pressure(), p.xtilt(), p.ytilt(), p.rotation(), 10, canvasState);
	}
	m_brushEngine.endStroke();

	paintEngine->previewDabs(owner.activeLayer(), m_brushEngine.messages());
	m_brushEngine.clearMessages();
}

Line::Line(ToolController &owner)
	: ShapeTool(owner, LINE, QCursor(QPixmap(":cursors/line.png"), 1, 1))
{
}

void Line::motion(const canvas::Point& point, bool constrain, bool center)
{
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
	canvas::PointVector pv;
	pv.reserve(2);
	pv << canvas::Point(m_p1, 1) << canvas::Point(m_p2, 1);
	return pv;
}

Rectangle::Rectangle(ToolController &owner)
	: ShapeTool(owner, RECTANGLE, QCursor(QPixmap(":cursors/rectangle.png"), 1, 1))
{
}

canvas::PointVector Rectangle::pointVector() const
{
	canvas::PointVector pv;
	pv.reserve(5);
	pv << canvas::Point(m_p1, 1);
	pv << canvas::Point(m_p1.x(), m_p2.y(), 1);
	pv << canvas::Point(m_p2, 1);
	pv << canvas::Point(m_p2.x(), m_p1.y(), 1);
	pv << canvas::Point(m_p1.x(), m_p1.y(), 1);
	return pv;
}

Ellipse::Ellipse(ToolController &owner)
	: ShapeTool(owner, ELLIPSE, QCursor(QPixmap(":cursors/ellipse.png"), 1, 1))
{
}

canvas::PointVector Ellipse::pointVector() const
{
	const auto r = rect();
	const qreal a = r.width() / 2.0;
	const qreal b = r.height() / 2.0;
	const qreal cx = r.x() + a;
	const qreal cy = r.y() + b;

	canvas::PointVector pv;

	// TODO smart step size selection
	for(qreal t=0;t<2*M_PI;t+=M_PI/20) {
		pv << canvas::Point(cx + a*cos(t), cy + b*sin(t), 1.0);
	}
	pv << canvas::Point(cx+a, cy, 1);
	return pv;
}

}

