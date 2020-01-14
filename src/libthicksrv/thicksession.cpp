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

#include "thicksession.h"

#include "../libserver/inmemoryhistory.h"
#include "../libserver/serverlog.h"
#include "../libserver/client.h"

#include "../libshared/net/control.h"
#include "../libshared/net/meta.h"

#include "../libclient/canvas/aclfilter.h"
#include "../libclient/canvas/statetracker.h"
#include "../libclient/canvas/layerlist.h"
#include "../libclient/canvas/loader.h"
#include "../libclient/core/layerstack.h"

namespace server {

ThickSession::ThickSession(ServerConfig *config, sessionlisting::Announcements *announcements, const QString &id, const QString &idAlias, const QString &founder, QObject *parent)
	: Session(
		new InMemoryHistory(id, idAlias, protocol::ProtocolVersion::current(), founder),
	    config, announcements, parent
		)
{
	history()->setParent(this);

	m_aclfilter = new canvas::AclFilter(this);
	m_statetracker = new canvas::StateTracker(
			new paintcore::LayerStack(this),
			new canvas::LayerListModel(this),
			0,
			this);
}

ThickSession::ThickSession(ServerConfig *config, sessionlisting::Announcements *announcements, canvas::StateTracker *statetracker, const canvas::AclFilter *aclFilter, const QString &id, const QString &idAlias, const QString &founder, QObject *parent)
	: Session(
		new InMemoryHistory(id, idAlias, protocol::ProtocolVersion::current(), founder),
		config, announcements, parent
		),
	  m_statetracker(statetracker)
{
	history()->setParent(this);
	m_aclfilter = aclFilter->clone(this);
}

void ThickSession::readyToAutoReset(int ctxId)
{
	log(Log().about(Log::Level::Warn, Log::Topic::RuleBreak).message(QString("User %1 sent ready-to-autoreset, but this is a thick server!").arg(ctxId)));
}

void ThickSession::addToHistory(protocol::MessagePtr msg)
{
	if(state() == State::Shutdown)
		return;

	if(!m_aclfilter->filterMessage(*msg)) {
		log(Log()
			.about(Log::Level::Warn, Log::Topic::RuleBreak)
			.message(msg->messageName() + " filtered")
		   );
		return;
	}

	// Special messages
	if(msg->type() == protocol::MSG_CHAT) {
		const auto chat = msg.cast<protocol::Chat>();
		if(chat.isPin()) {
			m_pinnedMessage = chat.message();
			if(m_pinnedMessage == "-")
				m_pinnedMessage = QString();
		}
	} else if(msg->type() == protocol::MSG_LAYER_DEFAULT) {
		m_defaultLayer = msg->layer();
	}

	// Execute commands only in self-contained mode.
	if(msg->isCommand() && m_statetracker->parent() == this)
		m_statetracker->receiveCommand(msg);

	addedToHistory(msg);

	if(state() == State::Initialization) {
		// Send to everyone except the initializing user
		for(Client *client : clients()) {
			if(client->id() != initUserId())
				client->sendDirectMessage(msg);
		}

	} else {
		for(Client *client : clients())
			client->sendDirectMessage(msg);
	}
}

void ThickSession::onSessionReset()
{
	protocol::MessageList msgs;
	int lastBatchIndex=0;
	std::tie(msgs, lastBatchIndex) = history()->getBatch(-1);

	Q_ASSERT(lastBatchIndex == history()->lastIndex()); // InMemoryHistory always returns the whole history

	history()->reset(protocol::MessageList());

	// Reset ACL filter state
	m_aclfilter->reset(m_statetracker->localId(), false);
	for(const auto &msg : msgs)
		m_aclfilter->filterMessage(*msg);

	protocol::ServerReply catchup;
	catchup.type = protocol::ServerReply::CATCHUP;
	catchup.reply["count"] = msgs.size();
	const auto catchupmsg = protocol::MessagePtr(new protocol::Command(0, catchup));

	for(Client *client : clients()) {
		client->sendDirectMessage(catchupmsg);
		client->sendDirectMessage(msgs);
	}
}

void ThickSession::onClientJoin(Client *client, bool host)
{
	if(host)
		return;

	directToAll(protocol::MessagePtr(new protocol::SoftResetPoint(m_statetracker->localId())));
	internalReset();

	protocol::MessageList msgs;
	int lastBatchIndex=0;
	std::tie(msgs, lastBatchIndex) = history()->getBatch(-1);

	Q_ASSERT(lastBatchIndex == history()->lastIndex()); // InMemoryHistory always returns the whole history

	history()->reset(protocol::MessageList());

	protocol::ServerReply catchup;
	catchup.type = protocol::ServerReply::CATCHUP;
	catchup.reply["count"] = msgs.size();

	client->sendDirectMessage(protocol::MessagePtr(new protocol::Command(0, catchup)));
	client->sendDirectMessage(msgs);
}

void ThickSession::internalReset()
{
	auto loader =  canvas::SnapshotLoader(
			m_statetracker->localId(),
			m_statetracker->image(),
			m_aclfilter
	);

	loader.setDefaultLayer(m_defaultLayer);
	loader.setPinnedMessage(m_pinnedMessage);

	auto messages = serverSideStateMessages() + loader.loadInitCommands();

	history()->reset(messages);

	log(Log()
		.about(Log::Level::Info, Log::Topic::Status)
		.message(QStringLiteral("Performed internal reset. Image size is %1 MB").arg(history()->sizeInBytes() / 1024.0 / 1024.0, 0, 'f', 2))
	   );
}

}

