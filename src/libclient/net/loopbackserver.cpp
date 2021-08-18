/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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

#include "loopbackserver.h"

namespace net {

LoopbackServer::LoopbackServer(QObject *parent)
	: Server(true, parent)
{
}

void LoopbackServer::logout()
{
	qWarning("tried to log out from the loopback server!");
}

void LoopbackServer::sendMessage(const protocol::MessagePtr &msg)
{
	emit messageReceived(msg);
}

void LoopbackServer::sendMessages(const protocol::MessageList &msgs)
{
	for(const protocol::MessagePtr &msg : msgs)
		emit messageReceived(msg);
}

void LoopbackServer::sendEnvelope(const Envelope &envelope)
{
	// FIXME we want to pass around only envelopes
	Envelope e = envelope;
	while(!e.isEmpty()) {
		protocol::NullableMessageRef m = protocol::Message::deserialize(e.data(), e.length(), true);
		Q_ASSERT(!m.isNull());
		emit messageReceived(protocol::MessagePtr::fromNullable(m));
		e = e.next();
	}
}

}
