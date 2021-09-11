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

#include "brushes/shapes.h"
#include "net/client.h"
#include "net/envelopebuilder.h"

#include "tools/toolcontroller.h"
#include "tools/shapetools.h"
#include "tools/utils.h"

#include "../rustpile/rustpile.h"

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
	rustpile::paintengine_remove_preview(owner.model()->paintEngine()->engine(), owner.activeLayer());
	m_drawing = false;
}

void ShapeTool::end()
{
	if(!m_drawing)
		return;

	m_drawing = false;

	const uint8_t contextId = owner.client()->myId();
	auto engine = owner.model()->paintEngine()->engine();

	auto brushengine = rustpile::brushengine_new();

	rustpile::brushengine_set_classicbrush(brushengine, &owner.activeBrush().brush(), owner.activeLayer());

	const auto pv = pointVector();
	for(const auto &p : pv) {
		rustpile::brushengine_stroke_to(brushengine, p.x(), p.y(), p.pressure(), engine, owner.activeLayer());
	}
	rustpile::brushengine_end_stroke(brushengine);

	net::EnvelopeBuilder writer;
	rustpile::write_undopoint(writer, contextId);
	rustpile::brushengine_write_dabs(brushengine, contextId, writer);
	rustpile::write_penup(writer, contextId);

	rustpile::brushengine_free(brushengine);

	rustpile::paintengine_remove_preview(engine, owner.activeLayer());
	owner.client()->sendEnvelope(writer.toEnvelope());
}

void ShapeTool::updatePreview()
{
	auto brushengine = rustpile::brushengine_new();

	rustpile::brushengine_set_classicbrush(brushengine, &owner.activeBrush().brush(), owner.activeLayer());

	auto engine = owner.model()->paintEngine()->engine();

	const canvas::PointVector pv = pointVector();
	Q_ASSERT(pv.size()>1);

	for(const auto &p : pv) {
		rustpile::brushengine_stroke_to(brushengine, p.x(), p.y(), p.pressure(), engine, owner.activeLayer());
	}
	rustpile::brushengine_end_stroke(brushengine);

	rustpile::paintengine_preview_brush(engine, owner.activeLayer(), brushengine);
	rustpile::brushengine_free(brushengine);
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
	return brushes::shapes::rectangle(rect());
}

Ellipse::Ellipse(ToolController &owner)
	: ShapeTool(owner, ELLIPSE, QCursor(QPixmap(":cursors/ellipse.png"), 1, 1))
{
}

canvas::PointVector Ellipse::pointVector() const
{
	return brushes::shapes::ellipse(rect());
}

}

