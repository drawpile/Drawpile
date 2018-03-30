/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2017 Calle Laakkonen

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

#include "core/brush.h"
#include "canvas/canvasmodel.h"
#include "net/client.h"
#include "net/commands.h"

#include "tools/toolcontroller.h"
#include "tools/brushes.h"

#include "../shared/net/undo.h"
#include "../shared/net/pen.h"

#include <QPixmap>

namespace tools {

Freehand::Freehand(ToolController &owner, bool isEraser)
	: Tool(owner, isEraser ? ERASER : FREEHAND, Qt::CrossCursor)
{
}

void Freehand::begin(const paintcore::Point& point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	Q_UNUSED(right);

	m_brush.setBrush(owner.activeBrush());
	m_brush.setLayer(owner.activeLayer());
	m_brush.setContextId(owner.client()->myId());

	m_brush.strokeTo(point, nullptr);

	QList<protocol::MessagePtr> msgs;
	msgs << protocol::MessagePtr(new protocol::UndoPoint(owner.client()->myId()));

	msgs << net::command::brushToToolChange(owner.client()->myId(), owner.activeLayer(), owner.activeBrush());
	protocol::PenPointVector v(1);
	v[0] = net::command::pointToProtocol(point);
	msgs << protocol::MessagePtr(new protocol::PenMove(owner.client()->myId(), v));
	owner.client()->sendMessages(msgs);

}

void Freehand::motion(const paintcore::Point& point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);

	protocol::PenPointVector v(1);
	v[0] = net::command::pointToProtocol(point);
	owner.client()->sendMessage(protocol::MessagePtr(new protocol::PenMove(owner.client()->myId(), v)));

	const paintcore::Layer *srcLayer = nullptr;
	if(owner.activeBrush().smudge1()>0)
		srcLayer = owner.model()->layerStack()->getLayer(owner.activeLayer());

	m_brush.strokeTo(point, srcLayer);
	owner.client()->sendMessages(m_brush.takeDabs());
}

void Freehand::end()
{
	owner.client()->sendMessages(m_brush.takeDabs());
	owner.client()->sendMessage(protocol::MessagePtr(new protocol::PenUp(owner.client()->myId())));
	m_brush.endStroke();
}

}

