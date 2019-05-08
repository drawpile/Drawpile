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

#include "../shared/server/inmemoryhistory.h"
#include "../shared/server/serverlog.h"
#include "../shared/server/client.h"

#include "../shared/net/control.h"
#include "../shared/net/meta.h"

#include "../client/canvas/aclfilter.h"
#include "../client/canvas/statetracker.h"
#include "../client/canvas/layerlist.h"
#include "../client/canvas/loader.h"
#include "../client/core/layerstack.h"

namespace server {

ThickSession::ThickSession(ServerConfig *config, sessionlisting::Announcements *announcements, const QUuid &id, const QString &idAlias, const QString &founder, QObject *parent)
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

	qInfo("Adding: %s", qPrintable(msg->toString()));

	if(msg->isCommand())
			m_statetracker->receiveCommand(msg);

#if 0 // TODO history size limit
	history()->addMessage(msg);
#endif

	addedToHistory(msg);

	for(Client *client : clients())
		client->sendDirectMessage(msg);
}

void ThickSession::onSessionReset()
{
	protocol::MessageList msgs;
	int lastBatchIndex=0;
	std::tie(msgs, lastBatchIndex) = history()->getBatch(-1);

	Q_ASSERT(lastBatchIndex == history()->lastIndex()); // InMemoryHistory always returns the whole history

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

	// TODO reset only if needed
	internalReset();

	protocol::MessageList msgs;
	int lastBatchIndex=0;
	std::tie(msgs, lastBatchIndex) = history()->getBatch(-1);

	Q_ASSERT(lastBatchIndex == history()->lastIndex()); // InMemoryHistory always returns the whole history

	protocol::ServerReply catchup;
	catchup.type = protocol::ServerReply::CATCHUP;
	catchup.reply["count"] = msgs.size();
	client->sendDirectMessage(protocol::MessagePtr(new protocol::Command(0, catchup)));
	
	qInfo("Sending history:");
	for(protocol::MessagePtr m : msgs)
		qInfo("> %s", qPrintable(m->toString()));

	client->sendDirectMessage(msgs);
}

void ThickSession::internalReset()
{
	// Inform everyone that a soft-reset just occurred
	directToAll(protocol::MessagePtr(new protocol::SoftResetPoint(0)));

	auto loader =  canvas::SnapshotLoader(
			0,
			m_statetracker->image(),
			m_aclfilter
	);

	loader.setDefaultLayer(m_statetracker->layerList()->defaultLayer());
	loader.setPinnedMessage(QString()); // TODO

	auto messages = loader.loadInitCommands();

	QList<uint8_t> owners;
	QList<uint8_t> trusted;

	for(const Client * c : clients()) {
		if(c->isOperator())
			owners << c->id();
		if(c->isTrusted())
			trusted << c->id();
	}

	messages << protocol::MessagePtr(new protocol::SessionOwner(0, owners));
	if(!trusted.isEmpty())
		messages << protocol::MessagePtr(new protocol::TrustedUsers(0, trusted));

	history()->reset(messages);

	log(Log()
		.about(Log::Level::Info, Log::Topic::Status)
		.message(QStringLiteral("Performed internal reset. Image size is %1 MB").arg(history()->sizeInBytes() / 1024.0 / 1024.0, 0, 'f', 2))
	   );
}

}

