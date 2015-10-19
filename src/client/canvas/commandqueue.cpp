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

namespace canvas {

CommandQueue::CommandQueue(CanvasModel *canvas, QObject *parent)
	: QObject(parent), m_canvas(canvas)
{
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
		case MSG_CHAT:
			m_canvas->metaChat(cmd.cast<Chat>());
			break;
		case MSG_USER_JOIN:
			m_canvas->metaUserJoin(cmd.cast<UserJoin>());
			break;
		case MSG_USER_LEAVE:
			m_canvas->metaUserLeave(cmd.cast<UserLeave>());
			break;
		case MSG_SESSION_OWNER:
			// TODO
			//m_canvas->m_userlist->updateOperators(cmd->contextId(), cmd.cast<protocol::SessionOwner>().ids());
			break;
		case MSG_USER_ACL:
			// TODO
			//m_canvas->m_userlist->updateLocks(cmd.cast<protocol::UserACL>().ids());
			break;
		case MSG_SESSION_ACL:
		case MSG_LAYER_ACL:
			// Handled by the ACL filter
			break;
		case MSG_INTERVAL:
			/* intervals are used only when playing back recordings */
			break;
		case MSG_LASERTRAIL:
			m_canvas->metaLaserTrail(cmd.cast<protocol::LaserTrail>());
			break;
		case MSG_MOVEPOINTER:
			m_canvas->metaMovePointer(cmd.cast<MovePointer>());
			break;
		case MSG_MARKER:
			m_canvas->metaMarkerMessage(cmd.cast<Marker>());
			break;
		default:
			qWarning("Unhandled meta message type %d", cmd->type());
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
