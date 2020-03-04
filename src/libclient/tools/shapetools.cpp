/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2019 Calle Laakkonen

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
#include "brushes/brushpainter.h"
#include "net/client.h"
#include "net/commands.h"

#include "tools/toolcontroller.h"
#include "tools/shapetools.h"
#include "tools/utils.h"

#include "../libshared/net/undo.h"

#include <QPixmap>

namespace tools {

void ShapeTool::begin(const paintcore::Point& point, bool right, float zoom)
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

void ShapeTool::motion(const paintcore::Point& point, bool constrain, bool center)
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
#if 0 // FIXME
	auto layers = owner.model()->layerStack()->editor(owner.client()->myId());
	auto layer = layers.getEditableLayer(owner.activeLayer());

	if(!layer.isNull()) {
		layer.removeSublayer(-1);
	}

	m_drawing = false;
#endif
}

void ShapeTool::end()
{
#if 0 // FIXME
	if(!m_drawing)
		return;

	m_drawing = false;

	const uint8_t contextId = owner.client()->myId();

	auto layers = owner.model()->layerStack()->editor(contextId);
	auto layer = layers.getEditableLayer(owner.activeLayer());

	if(!layer.isNull()) {
		layer.removeSublayer(-1);
	}

	brushes::BrushEngine brushengine;
	brushengine.setBrush(owner.client()->myId(), owner.activeLayer(), owner.activeBrush());

	const auto pv = pointVector();
	for(int i=0;i<pv.size();++i)
		brushengine.strokeTo(pv.at(i), layer.layer());
	brushengine.endStroke();

	protocol::MessageList msgs;
	msgs << protocol::MessagePtr(new protocol::UndoPoint(contextId));
	msgs << brushengine.takeDabs();
	msgs << protocol::MessagePtr(new protocol::PenUp(contextId));
	owner.client()->sendMessages(msgs);
#endif
}

void ShapeTool::updatePreview()
{
#if 0 // FIXME
	auto layers = owner.model()->layerStack()->editor(0);
	auto layer = layers.getEditableLayer(owner.activeLayer());
	if(layer.isNull()) {
		qWarning("ShapeTool::updatePreview: no active layer!");
		return;
	}

	const paintcore::PointVector pv = pointVector();
	Q_ASSERT(pv.size()>1);

	brushes::BrushEngine brushengine;
	brushengine.setBrush(0, 0, owner.activeBrush());

	for(int i=0;i<pv.size();++i)
		brushengine.strokeTo(pv.at(i), layer.layer());
	brushengine.endStroke();

	layer.removeSublayer(-1);

	const auto dabs = brushengine.takeDabs();
	for(int i=0;i<dabs.size();++i)
		brushes::drawBrushDabsDirect(*dabs.at(i), layer, -1);
#endif
}

Line::Line(ToolController &owner)
	: ShapeTool(owner, LINE, QCursor(QPixmap(":cursors/line.png"), 1, 1))
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
	: ShapeTool(owner, RECTANGLE, QCursor(QPixmap(":cursors/rectangle.png"), 1, 1))
{
}

paintcore::PointVector Rectangle::pointVector() const
{
	return brushes::shapes::rectangle(rect());
}

Ellipse::Ellipse(ToolController &owner)
	: ShapeTool(owner, ELLIPSE, QCursor(QPixmap(":cursors/ellipse.png"), 1, 1))
{
}

paintcore::PointVector Ellipse::pointVector() const
{
	return brushes::shapes::ellipse(rect());
}

}

