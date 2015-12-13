/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include "commandqueue.h"
#include "canvasmodel.h"
#include "aclfilter.h"
#include "statetracker.h"
#include "../shared/net/message.h"

#include "../shared/net/meta.h"
#include "../shared/net/meta2.h"
#include "../shared/net/recording.h"

#include <QMetaObject>

namespace canvas {

CommandQueue::CommandQueue(CanvasModel *canvas, QObject *parent)
	: QObject(parent), m_canvas(canvas)
{
	int idx = m_canvas->metaObject()->indexOfMethod("handleMeta(MessagePtr)");
	Q_ASSERT(idx>=0);
	m_metahandler = m_canvas->metaObject()->method(idx);
}

bool CommandQueue::event(QEvent *e)
{
	if(e->type() == CommandEvent::commandEventType()) {
		CommandEvent *ce = static_cast<CommandEvent*>(e);
		if(ce->isLocal())
			handleLocalCommand(ce->message());
		else
			handleCommand(ce->message());

		return true;
	}
	return QObject::event(e);
}

void CommandQueue::handleCommand(protocol::MessagePtr cmd)
{
	using namespace protocol;

	// Apply ACL filter
	if(!m_canvas->m_aclfilter->filterMessage(*cmd)) {
		qDebug("Filtered message %d from %d", cmd->type(), cmd->contextId());
		return;
	}

	if(cmd->isMeta()) {
		// Handle meta commands here
		switch(cmd->type()) {
		case MSG_SESSION_ACL:
		case MSG_LAYER_ACL:
			// Handled by the ACL filter
			break;
		case MSG_INTERVAL:
			// intervals are used only when playing back recordings
			break;
		default:
			// Rest of the META messages affect things that are shown
			// in the UI, so they are best handled back in the main thread.
			m_metahandler.invoke(m_canvas, Qt::AutoConnection, Q_ARG(protocol::MessagePtr, cmd));
		}

	} else if(cmd->isCommand()) {
		// The state tracker handles all drawing commands
		m_canvas->m_statetracker->receiveCommand(cmd);
		emit m_canvas->canvasModified();

	} else {
		qWarning("CanvasModel::handleDrawingCommand: command %d is neither Meta nor Command type!", cmd->type());
	}
}

void CommandQueue::handleLocalCommand(protocol::MessagePtr msg)
{
	Q_ASSERT(msg->isCommand());
	m_canvas->m_statetracker->localCommand(msg);
	emit m_canvas->canvasModified();
}

}
