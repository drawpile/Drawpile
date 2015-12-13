/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2015 Calle Laakkonen

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
#include "core/shapes.h"
#include "net/client.h"
#include "net/commands.h"

#include "tools/toolcontroller.h"
#include "tools/shapetools.h"
#include "tools/utils.h"

#include "../shared/net/pen.h"
#include "../shared/net/undo.h"

#include <QPixmap>

namespace tools {

void ShapeTool::begin(const paintcore::Point& point, float zoom)
{
	Q_UNUSED(zoom);

	m_start = point;
	m_p1 = point;
	m_p2 = point;
	m_brush = owner.activeBrush();

	updatePreview();
}

void ShapeTool::motion(const paintcore::Point& point, bool constrain, bool center)
{
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

void ShapeTool::end()
{
	paintcore::LayerStack::Locker(owner.model()->layerStack());
	paintcore::Layer *layer = owner.model()->layerStack()->getLayer(owner.activeLayer());
	if(layer) {
		layer->removeSublayer(-1);
	}
	
	QList<protocol::MessagePtr> msgs;
	msgs << protocol::MessagePtr(new protocol::UndoPoint(0));
	msgs << net::command::brushToToolChange(0, owner.activeLayer(), owner.activeBrush());
	msgs << net::command::penMove(0, pointVector());
	msgs << protocol::MessagePtr(new protocol::PenUp(0));
	owner.client()->sendMessages(msgs);
}

void ShapeTool::updatePreview()
{
	paintcore::LayerStack::Locker(owner.model()->layerStack());
	paintcore::Layer *layer = owner.model()->layerStack()->getLayer(owner.activeLayer());
	if(layer) {
		paintcore::StrokeState ss;
		layer->removeSublayer(-1);

		const paintcore::PointVector pv = pointVector();
		Q_ASSERT(pv.size()>1);

		layer->dab(-1, m_brush, pv[0], ss);

		for(int i=1;i<pv.size();++i)
			layer->drawLine(-1, m_brush, pv[i-1], pv[i], ss);
	}
}

Line::Line(ToolController &owner)
	: ShapeTool(owner, LINE, QCursor(QPixmap(":cursors/line.png"), 2, 2))
{
}

void Line::motion(const paintcore::Point& point, bool constrain, bool center)
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

paintcore::PointVector Line::pointVector() const
{
	paintcore::PointVector pv;
	pv.reserve(2);
	pv << paintcore::Point(m_p1, 1) << paintcore::Point(m_p2, 1);
	return pv;
}

Rectangle::Rectangle(ToolController &owner)
	: ShapeTool(owner, RECTANGLE, QCursor(QPixmap(":cursors/rectangle.png"), 2, 2))
{
}

paintcore::PointVector Rectangle::pointVector() const
{
	return paintcore::shapes::rectangle(rect());
}

Ellipse::Ellipse(ToolController &owner)
	: ShapeTool(owner, ELLIPSE, QCursor(QPixmap(":cursors/ellipse.png"), 2, 2))
{
}

paintcore::PointVector Ellipse::pointVector() const
{
	return paintcore::shapes::ellipse(rect());
}

}

