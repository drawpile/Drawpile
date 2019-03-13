/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2018 Calle Laakkonen

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
#include "net/client.h"
#include "net/commands.h"

#include "tools/toolcontroller.h"
#include "tools/freehand.h"

#include "../shared/net/undo.h"

namespace tools {

Freehand::Freehand(ToolController &owner, bool isEraser)
	: Tool(owner, isEraser ? ERASER : FREEHAND, Qt::CrossCursor), m_drawing(false)
{
}

void Freehand::begin(const paintcore::Point& point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	Q_UNUSED(right);
	Q_ASSERT(!m_drawing);

	m_drawing = true;
	m_firstPoint = true;
	m_brushengine.setBrush(owner.client()->myId(), owner.activeLayer(), owner.activeBrush());

	// The pressure value of the first point is unreliable
	// because it is (or was?) possible to get a synthetic MousePress event
	// before the StylusPress event.
	m_start = point;
}

void Freehand::motion(const paintcore::Point& point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);
	if(!m_drawing)
		return;

	const paintcore::Layer *srcLayer = nullptr;
	if(owner.activeBrush().smudge1()>0)
		srcLayer = owner.model()->layerStack()->getLayer(owner.activeLayer());

	if(m_firstPoint) {
		m_firstPoint = false;
		m_brushengine.strokeTo(paintcore::Point(m_start, qMin(m_start.pressure(), point.pressure())), srcLayer);
		owner.client()->sendMessage(protocol::MessagePtr(new protocol::UndoPoint(owner.client()->myId())));
	}

	m_brushengine.strokeTo(point, srcLayer);
	owner.client()->sendMessages(m_brushengine.takeDabs());
}

void Freehand::end()
{
	if(m_drawing) {
		m_drawing = false;

		if(m_firstPoint) {
			m_firstPoint = false;
			m_brushengine.strokeTo(m_start, nullptr);
			owner.client()->sendMessage(protocol::MessagePtr(new protocol::UndoPoint(owner.client()->myId())));
		}

		m_brushengine.endStroke();
		QList<protocol::MessagePtr> msgs = m_brushengine.takeDabs();
		msgs << protocol::MessagePtr(new protocol::PenUp(owner.client()->myId()));
		owner.client()->sendMessages(msgs);
	}
}

}

