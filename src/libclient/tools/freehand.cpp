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

extern "C" {
#include <dpcommon/queue.h>
#include <dpmsg/message.h>
}

#include "canvas/canvasmodel.h"
#include "canvas/paintengine.h"
#include "drawdance/canvasstate.h"
#include "net/client.h"

#include "tools/toolcontroller.h"
#include "tools/freehand.h"

#include "../libshared/net/undo.h"

namespace tools {

Freehand::Freehand(ToolController &owner, bool isEraser)
	: Tool(owner, isEraser ? ERASER : FREEHAND, Qt::CrossCursor)
	, m_brushEngine{}
	, m_drawing(false)
{
}

Freehand::~Freehand()
{
}

void Freehand::begin(const canvas::Point& point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	Q_UNUSED(right);
	Q_ASSERT(!m_drawing);

	m_drawing = true;
	m_firstPoint = true;
	m_lastTimestamp = QDateTime::currentMSecsSinceEpoch();

	owner.setBrushEngineBrush(m_brushEngine);

	// The pressure value of the first point is unreliable
	// because it is (or was?) possible to get a synthetic MousePress event
	// before the StylusPress event.
	m_start = point;
}

void Freehand::motion(const canvas::Point& point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);
	if(!m_drawing)
		return;

	drawdance::CanvasState canvasState = owner.model()->paintEngine()->viewCanvasState();

	if(m_firstPoint) {
		m_firstPoint = false;
		m_brushEngine.beginStroke(owner.client()->myId());
		m_brushEngine.strokeTo(
			m_start.x(),
			m_start.y(),
			qMin(m_start.pressure(), point.pressure()),
			m_start.xtilt(),
			m_start.ytilt(),
			0,
			canvasState);
	}

	qint64 now = QDateTime::currentMSecsSinceEpoch();
	qint64 deltaMsec = now - m_lastTimestamp;
	m_lastTimestamp = now;

	m_brushEngine.strokeTo(
		point.x(),
		point.y(),
		point.pressure(),
		point.xtilt(),
		point.ytilt(),
		deltaMsec,
		canvasState);

	m_brushEngine.sendMessagesTo(owner.client());
}

void Freehand::end()
{
	if(m_drawing) {
		m_drawing = false;

		if(m_firstPoint) {
			m_firstPoint = false;
			m_brushEngine.beginStroke(owner.client()->myId());
			m_brushEngine.strokeTo(
				m_start.x(),
				m_start.y(),
				m_start.pressure(),
				m_start.xtilt(),
				m_start.ytilt(),
				QDateTime::currentMSecsSinceEpoch(),
				drawdance::CanvasState::null());
		}

		m_brushEngine.endStroke();
		m_brushEngine.sendMessagesTo(owner.client());
	}
}

void Freehand::offsetActiveTool(int x, int y)
{
	if(m_drawing) {
		m_brushEngine.addOffset(x, y);
	}
}

}

