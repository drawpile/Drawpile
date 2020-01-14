/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#include "builtinsession.h"
#include "../libserver/client.h"
#include "../libshared/net/meta.h"
#include "../libshared/net/control.h"
#include "../libclient/canvas/statetracker.h"

namespace server {

BuiltinSession::BuiltinSession(ServerConfig *config, sessionlisting::Announcements *announcements, canvas::StateTracker *statetracker, const canvas::AclFilter *aclFilter, const QString &id, const QString &idAlias, const QString &founder, QObject *parent)
	: ThickSession(config, announcements, statetracker, aclFilter, id, idAlias, founder, parent)
{
}

void BuiltinSession::onClientJoin(Client *client, bool host)
{
	if(host) {
		switchState(State::Running);
		return;
	}

	// New client must wait until soft reset is processed.
	// We can't do it right away, since the client's statetracker processes messages asynchronously.
	client->setAwaitingReset(true);

	// Just send the softresetpoint. The StateTracker will emit softResetPoint, which should be connected
	// to our doInternalResetNow slot.
	if(!m_softResetRequested) {
		directToAll(protocol::MessagePtr(new protocol::SoftResetPoint(stateTracker()->localId())));
		m_softResetRequested = true;
	}
}

void BuiltinSession::doInternalResetNow()
{
	m_softResetRequested = false;

	QList<Client*> awaitingClients;
	for(Client *c : clients()) {
		if(c->isAwaitingReset())
			awaitingClients << c;
	}

	if(awaitingClients.isEmpty()) {
		qWarning("doInternalResetNow(): no clients awaiting reset, not doing it...");
		return;
	}

	internalReset();

	protocol::MessageList msgs;
	int lastBatchIndex=0;
	std::tie(msgs, lastBatchIndex) = history()->getBatch(-1);

	Q_ASSERT(lastBatchIndex == history()->lastIndex()); // InMemoryHistory always returns the whole history

	history()->reset(protocol::MessageList());

	protocol::ServerReply catchup;
	catchup.type = protocol::ServerReply::CATCHUP;
	catchup.reply["count"] = msgs.size();
	const auto catchupMsg = protocol::MessagePtr(new protocol::Command(0, catchup));

	for(Client *c : awaitingClients) {
		c->setAwaitingReset(false);
		c->sendDirectMessage(catchupMsg);
		c->sendDirectMessage(msgs);
	}
}

}
