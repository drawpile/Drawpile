// SPDX-License-Identifier: GPL-3.0-or-later

#include "libserver/thinsession.h"
#include "libserver/thinserverclient.h"
#include "libserver/serverlog.h"
#include "libserver/serverconfig.h"

#include "libshared/net/control.h"

namespace server {

ThinSession::ThinSession(SessionHistory *history, ServerConfig *config, sessionlisting::Announcements *announcements, QObject *parent)
	: Session(history, config, announcements, parent)
{
	history->setSizeLimit(config->getConfigSize(config::SessionSizeLimit));
	history->setAutoResetThreshold(config->getConfigSize(config::AutoresetThreshold));
	m_lastStatusUpdate.start();
}

void ThinSession::addToHistory(protocol::MessagePtr msg)
{
	if(state() == State::Shutdown)
		return;

	// Add message to history (if there is space)
	if(!history()->addMessage(msg)) {
		messageAll("History size limit reached! Session must be reset to continue.", false);
		return;
	}

	// The hosting user must skip the history uploaded during initialization
	// (since they originated it), but we still want to send them notifications.
	if(state() == State::Initialization) {
		Client *origin = getClientById(initUserId());
		Q_ASSERT(origin);
		if(origin) {
			static_cast<ThinServerClient*>(origin)->setHistoryPosition(history()->lastIndex());
			if(!msg->isCommand())
				origin->sendDirectMessage(msg);
		}
	}

	addedToHistory(msg);

	// Request auto-reset when threshold is crossed.
	const uint autoResetThreshold = history()->effectiveAutoResetThreshold();
	if(autoResetThreshold>0 && m_autoResetRequestStatus == AutoResetState::NotSent && history()->sizeInBytes() > autoResetThreshold) {
		log(Log().about(Log::Level::Info, Log::Topic::Status).message(
			QString("Autoreset threshold (%1, effectively %2 MB) reached.")
				.arg(history()->autoResetThreshold()/(1024.0*1024.0), 0, 'g', 1)
				.arg(autoResetThreshold/(1024.0*1024.0), 0, 'g', 1)
		));

		// Legacy alert for Drawpile 2.0.x versions
		protocol::ServerReply warning;
		warning.type = protocol::ServerReply::SIZELIMITWARNING;
		warning.reply["size"] = int(history()->sizeInBytes());
		warning.reply["maxSize"] = int(autoResetThreshold);

		directToAll(protocol::MessagePtr(new protocol::Command(0, warning)));

		// New style for Drawpile 2.1.0 and newer
		// Autoreset request: send an autoreset query to each logged in operator.
		// The user that responds first gets to perform the reset.
		protocol::ServerReply resetRequest;
		resetRequest.type = protocol::ServerReply::RESETREQUEST;
		resetRequest.reply["maxSize"] = int(history()->sizeLimit());
		resetRequest.reply["query"] = true;
		protocol::MessagePtr reqMsg { new protocol::Command(0, resetRequest )};

		for(Client *c : clients()) {
			if(c->isOperator())
				c->sendDirectMessage(reqMsg);
		}

		m_autoResetRequestStatus = AutoResetState::Queried;
	}

	// Regular history size status updates
	if(m_lastStatusUpdate.elapsed() > 10 * 1000) {
		protocol::ServerReply status;
		status.type = protocol::ServerReply::STATUS;
		status.reply["size"] = int(history()->sizeInBytes());
		directToAll(protocol::MessagePtr(new protocol::Command(0, status)));
		m_lastStatusUpdate.start();
	}
}

void ThinSession::cleanupHistoryCache()
{
	int minIdx = history()->lastIndex();
	for(const Client *c : clients()) {
		minIdx = qMin(static_cast<const ThinServerClient*>(c)->historyPosition(), minIdx);
	}
	history()->cleanupBatches(minIdx);
}

void ThinSession::readyToAutoReset(int ctxId)
{
	Client *c = getClientById(ctxId);
	if(!c) {
		// Shouldn't happen
		log(Log().about(Log::Level::Error, Log::Topic::RuleBreak).message(QString("Non-existent user %1 sent ready-to-autoreset").arg(ctxId)));
		return;
	}

	if(!c->isOperator()) {
		// Unlikely to happen normally, but possible if connection is
		// really slow and user is deopped at just the right moment
		log(Log().about(Log::Level::Warn, Log::Topic::RuleBreak).message(QString("User %1 is not an operator, but sent ready-to-autoreset").arg(ctxId)));
		return;
	}

	if(m_autoResetRequestStatus != AutoResetState::Queried) {
		// Only the first response in handled
		log(Log().about(Log::Level::Debug, Log::Topic::Status).message(QString("User %1 was late to respond to an autoreset request").arg(ctxId)));
		return;
	}

	log(Log().about(Log::Level::Info, Log::Topic::Status).message(QString("User %1 responded to autoreset request first").arg(ctxId)));

	protocol::ServerReply resetRequest;
	resetRequest.type = protocol::ServerReply::RESETREQUEST;
	resetRequest.reply["maxSize"] = int(history()->sizeLimit());
	resetRequest.reply["query"] = false;
	c->sendDirectMessage(protocol::MessagePtr { new protocol::Command(0, resetRequest )});

	m_autoResetRequestStatus = AutoResetState::Requested;
}

void ThinSession::onSessionReset()
{
	protocol::ServerReply catchup;
	catchup.type = protocol::ServerReply::CATCHUP;
	catchup.reply["count"] = history()->lastIndex() - history()->firstIndex();
	directToAll(protocol::MessagePtr(new protocol::Command(0, catchup)));

	m_autoResetRequestStatus = AutoResetState::NotSent;
}

void ThinSession::onClientJoin(Client *client, bool host)
{
	connect(history(), &SessionHistory::newMessagesAvailable,
		static_cast<ThinServerClient*>(client), &ThinServerClient::sendNextHistoryBatch);

	if(!host) {
		// Notify the client how many messages to expect (at least)
		// The client can use this information to display a progress bar during the login phase
		protocol::ServerReply catchup;
		catchup.type = protocol::ServerReply::CATCHUP;
		catchup.reply["count"] = history()->lastIndex() - history()->firstIndex();
		client->sendDirectMessage(protocol::MessagePtr(new protocol::Command(0, catchup)));
	}
}

}

