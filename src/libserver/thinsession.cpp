// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/thinsession.h"
#include "libserver/serverconfig.h"
#include "libserver/serverlog.h"
#include "libserver/thinserverclient.h"
#include "libshared/net/message.h"
#include "libshared/net/servercmd.h"
#include <QRandomGenerator>
#include <QTimer>

namespace server {

ThinSession::ThinSession(
	SessionHistory *history, ServerConfig *config,
	sessionlisting::Announcements *announcements, QObject *parent)
	: Session(history, config, announcements, parent)
	, m_autoResetTimer(new QTimer(this))
{
	history->setSizeLimit(config->getConfigSize(config::SessionSizeLimit));
	history->setAutoResetThreshold(
		config->getConfigSize(config::AutoresetThreshold));
	sendUpdatedSessionProperties();
	resetLastStatusUpdate();
	m_autoResetTimer->setTimerType(Qt::VeryCoarseTimer);
	m_autoResetTimer->setInterval(AUTORESET_RESPONSE_DELAY_MSECS);
	m_autoResetTimer->setSingleShot(true);
	connect(
		m_autoResetTimer, &QTimer::timeout, this,
		&ThinSession::triggerAutoReset);
}

void ThinSession::addToHistory(const net::Message &msg)
{
	if(state() == State::Shutdown)
		return;

	// Add message to history (if there is space)
	if(!history()->addMessage(msg)) {
		// We allow one extra MiB for the most important messages: joins
		// (without avatars), leaves and session owner changes. These are
		// required to get the session actually reset. If that runs out, the
		// user may be screwed, depending on the situation. But you have to do
		// something truly pathological to fill up the entire emergency space
		// like that, so it's not worth putting immense effort into handling it.
		net::Message em = msg.asEmergencyMessage();
		if(!em.isNull() && history()->addEmergencyMessage(em)) {
			addedToHistory(em);
		} else if(msg.isServerMeta()) {
			directToAll(msg);
		}

		if(m_lastSizeWarning.hasExpired() || msg.type() == DP_MSG_JOIN) {
			directToAll(net::ServerReply::makeOutOfSpace());
			keyMessageAll(
				QStringLiteral(
					"Session is out of space! To continue drawing, an "
					"operator must reset it to bring it down to a smaller "
					"size. This can be done via Session > Reset."),
				true, net::ServerReply::KEY_OUT_OF_SPACE);
			m_lastSizeWarning.setRemainingTime(10000);
		}

		return;
	}

	// The hosting user must skip the history uploaded during initialization
	// (since they originated it), but we still want to send them notifications.
	if(state() == State::Initialization) {
		Client *origin = getClientById(initUserId());
		if(origin) {
			static_cast<ThinServerClient *>(origin)->setHistoryPosition(
				history()->lastIndex());
			if(!msg.isInCommandRange())
				origin->sendDirectMessage(msg);
		}
	}

	addedToHistory(msg);
	checkAutoResetQuery();

	// Regular history size status updates
	if(m_lastStatusUpdate.hasExpired()) {
		sendStatusUpdate();
	}
}

void ThinSession::cleanupHistoryCache()
{
	long long minIdx = history()->lastIndex();
	for(const Client *c : clients()) {
		minIdx = qMin(
			static_cast<const ThinServerClient *>(c)->historyPosition(),
			minIdx);
	}
	history()->cleanupBatches(minIdx);
}

QJsonObject ThinSession::getDescription(bool full, bool invite) const
{
	QJsonObject o = Session::getDescription(full, invite);
	if(full) {
		QString sessionState;
		switch(state()) {
		case State::Initialization:
			sessionState = QStringLiteral("initialization");
			break;
		case State::Running:
			sessionState = QStringLiteral("running");
			break;
		case State::Reset:
			sessionState = QStringLiteral("reset");
			break;
		case State::Shutdown:
			sessionState = QStringLiteral("shutdown");
			break;
		}

		QString autoresetRequestStatus;
		switch(m_autoResetRequestStatus) {
		case AutoResetState::NotSent:
			autoresetRequestStatus = QStringLiteral("not sent");
			break;
		case AutoResetState::Queried:
			autoresetRequestStatus = QStringLiteral("queried");
			break;
		case AutoResetState::QueriedWaiting:
			autoresetRequestStatus = QStringLiteral("queried waiting");
			break;
		case AutoResetState::Requested:
			autoresetRequestStatus = QStringLiteral("requested");
			break;
		}

		const SessionHistory *hist = history();
		QJsonObject a = QJsonObject({
			{QStringLiteral("delay"), m_autoResetDelay.remainingTime()},
			{QStringLiteral("historyFirstIndex"), double(hist->firstIndex())},
			{QStringLiteral("historyLastIndex"), double(hist->lastIndex())},
			{QStringLiteral("requestStatus"), autoresetRequestStatus},
			{QStringLiteral("sessionState"), sessionState},
			{QStringLiteral("stream"), hist->getStreamedResetDescription()},
		});

		if(m_autoResetTimer->isActive()) {
			a[QStringLiteral("timer")] = m_autoResetTimer->remainingTime();
		}

		o[QStringLiteral("autoreset")] = a;
	}
	return o;
}

void ThinSession::readyToAutoReset(
	const AutoResetResponseParams &params, const QString &payload)
{
	Client *c = nullptr;
	bool haveOtherQueriedClients = false;
	for(Client *candidate : clients()) {
		if(candidate->id() == params.ctxId) {
			c = candidate;
		} else if(candidate->resetFlags().testFlag(
					  Client::ResetFlag::Queried)) {
			haveOtherQueriedClients = true;
		}
	}

	if(!c) {
		// Shouldn't happen
		log(Log()
				.about(Log::Level::Error, Log::Topic::RuleBreak)
				.message(QStringLiteral(
							 "Non-existent user %1 sent ready-to-autoreset")
							 .arg(params.ctxId)));
		return;
	}

	if(!c->isOperator() || c->isGhost()) {
		// Unlikely to happen normally, but possible if connection is
		// really slow and user is deopped at just the right moment
		c->setResetFlags(Client::ResetFlag::None);
		log(Log()
				.about(Log::Level::Warn, Log::Topic::RuleBreak)
				.message(QStringLiteral("User %1 is not an operator, but sent "
										"ready-to-autoreset")
							 .arg(params.ctxId)));
		return;
	}

	if(!c->resetFlags().testFlag(Client::ResetFlag::Queried)) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::RuleBreak)
				.message(QStringLiteral("User %1 responded to an autoreset "
										"request without being in query state")
							 .arg(params.ctxId)));
		return;
	}


	if(m_autoResetRequestStatus != AutoResetState::Queried &&
	   m_autoResetRequestStatus != AutoResetState::QueriedWaiting) {
		c->setResetFlags(Client::ResetFlag::None);
		log(Log()
				.about(Log::Level::Debug, Log::Topic::Status)
				.message(
					QStringLiteral(
						"User %1 was late to respond to an autoreset request")
						.arg(params.ctxId)));
		return;
	}

	if(!payload.isEmpty() && payload != m_autoResetPayload) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::RuleBreak)
				.message(QStringLiteral("User %1 responded with incorrect "
										"autoreset payload")
							 .arg(params.ctxId)));
		return;
	}

	int responseRank = m_autoResetCandidates.isEmpty()
						   ? 1
						   : m_autoResetCandidates.last().responseRank + 1;
	m_autoResetCandidates.append(AutoResetCandidate{
		responseRank, params.ctxId, params.capabilities, params.osQuality,
		params.netQuality, params.averagePing});

	c->setResetFlags(Client::ResetFlag::Responded);

	if(m_autoResetRequestStatus == AutoResetState::Queried) {
		m_autoResetRequestStatus = AutoResetState::QueriedWaiting;
		if(haveOtherQueriedClients) {
			m_autoResetTimer->start(AUTORESET_RESPONSE_DELAY_MSECS);
		} else {
			m_autoResetTimer->stop();
			triggerAutoReset();
		}
	} else if(!haveOtherQueriedClients) {
		m_autoResetTimer->stop();
		triggerAutoReset();
	}
}

StreamResetStartResult
ThinSession::handleStreamResetStart(int ctxId, const QString &correlator)
{
	if(m_autoResetRequestStatus != AutoResetState::Requested) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::RuleBreak)
				.message(QStringLiteral("User %1 tried to start stream reset, "
										"but session is in state %2")
							 .arg(ctxId)
							 .arg(int(m_autoResetRequestStatus))));
		return StreamResetStartResult::InvalidSessionState;
	}

	Client *c = getClientById(ctxId);
	if(!c) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::RuleBreak)
				.message(QStringLiteral("User %1 tried to start stream reset, "
										"but doesn't exist")
							 .arg(ctxId)));
		return StreamResetStartResult::InvalidUser;
	}

	Client::ResetFlags resetFlags = c->resetFlags();
	if(!resetFlags.testFlag(Client::ResetFlag::Awaiting) ||
	   !resetFlags.testFlag(Client::ResetFlag::Streaming)) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::RuleBreak)
				.message(QStringLiteral(
							 "User %1 tried to start stream reset, but we're "
							 "not waiting for a streamed reset from them")
							 .arg(ctxId)));
		return StreamResetStartResult::InvalidUser;
	}

	if(!c->isOperator() || c->isGhost()) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::RuleBreak)
				.message(QStringLiteral("User %1 tried to start stream reset, "
										"but isn't an operator")
							 .arg(ctxId)));
		clearAutoReset(AUTORESET_FAILURE_RETRY_MSECS);
		return StreamResetStartResult::InvalidUser;
	}

	if(correlator != m_autoResetPayload) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::RuleBreak)
				.message(
					QStringLiteral("User %1 tried to start stream reset, with "
								   "a correlator not matching payload")
						.arg(ctxId)));
		clearAutoReset(AUTORESET_FAILURE_RETRY_MSECS);
		return StreamResetStartResult::InvalidCorrelator;
	}

	SessionHistory *hist = history();
	StreamResetStartResult result =
		hist->startStreamedReset(ctxId, correlator, serverSideStateMessages());

	bool clear = false;
	QString error;
	switch(result) {
	case StreamResetStartResult::Ok:
		c->setResetFlags(
			result == StreamResetStartResult::Ok ? Client::ResetFlag::Streaming
												 : Client::ResetFlag::None);
		log(Log()
				.about(Log::Level::Info, Log::Topic::Status)
				.message(QStringLiteral("User %1 started stream reset (fork "
										"pos %2, stream pos %3)")
							 .arg(ctxId)
							 .arg(hist->resetStreamForkPos())
							 .arg(hist->resetStreamHeaderPos())));
		return StreamResetStartResult::Ok;
	case StreamResetStartResult::Unsupported:
		error = QStringLiteral("that's unsupported by this session");
		break;
	case StreamResetStartResult::InvalidSessionState:
		error = QStringLiteral("we're not anticipating one");
		break;
	case StreamResetStartResult::InvalidCorrelator:
		error = QStringLiteral("the correlator doesn't match");
		break;
	case StreamResetStartResult::InvalidUser:
		error = QStringLiteral("they're not the one tasked to do it");
		break;
	case StreamResetStartResult::AlreadyActive:
		error = QStringLiteral("it's already started");
		break;
	case StreamResetStartResult::OutOfSpace:
		error = QStringLiteral("the session is out of space");
		break;
	case StreamResetStartResult::WriteError:
		error = QStringLiteral("a write error occurred");
		break;
	}
	if(error.isEmpty()) {
		error = QStringLiteral("unknown error %1 occurred").arg(int(result));
	}

	log(Log()
			.about(Log::Level::Warn, Log::Topic::Status)
			.message(
				QStringLiteral("User %1 tried to start stream reset, but %2")
					.arg(ctxId)
					.arg(error)));

	if(clear) {
		clearAutoReset(AUTORESET_FAILURE_RETRY_MSECS);
	}

	return result;
}

StreamResetAbortResult ThinSession::handleStreamResetAbort(int ctxId)
{
	Client *c = getClientById(ctxId);
	if(!c) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::RuleBreak)
				.message(QStringLiteral("User %1 tried to abort stream reset, "
										"but doesn't exist")
							 .arg(ctxId)));
		return StreamResetAbortResult::InvalidUser;
	}

	StreamResetAbortResult result = history()->abortStreamedReset(ctxId);

	QString error;
	switch(result) {
	case StreamResetAbortResult::Ok:
		log(Log()
				.about(Log::Level::Info, Log::Topic::Status)
				.message(
					QStringLiteral("User %1 aborted stream reset").arg(ctxId)));
		clearAutoReset(AUTORESET_FAILURE_RETRY_MSECS);
		return StreamResetAbortResult::Ok;
	case StreamResetAbortResult::Unsupported:
		error = QStringLiteral("that's unsupported by this session");
		break;
	case StreamResetAbortResult::InvalidUser:
		error = QStringLiteral("isn't the one performing it");
		break;
	case StreamResetAbortResult::NotActive:
		error = QStringLiteral("none is active");
		break;
	}
	if(error.isEmpty()) {
		error = QStringLiteral("unknown error %1 occurred").arg(int(result));
	}

	log(Log()
			.about(Log::Level::Warn, Log::Topic::Status)
			.message(
				QStringLiteral("User %1 tried to abort stream reset, but %2")
					.arg(ctxId)
					.arg(error)));

	return result;
}

StreamResetPrepareResult
ThinSession::handleStreamResetFinish(int ctxId, int expectedMessageCount)
{
	Client *c = getClientById(ctxId);
	if(!c) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::RuleBreak)
				.message(QStringLiteral("User %1 tried to finish stream reset "
										"with %2 messages but doesn't exist")
							 .arg(ctxId)
							 .arg(expectedMessageCount)));
		return StreamResetPrepareResult::InvalidUser;
	}

	StreamResetPrepareResult result =
		history()->prepareStreamedReset(ctxId, expectedMessageCount);

	bool clear = false;
	QString error;
	switch(result) {
	case StreamResetPrepareResult::Ok:
		c->setResetFlags(Client::ResetFlag::None);
		log(Log()
				.about(Log::Level::Info, Log::Topic::Status)
				.message(QStringLiteral("User %1 prepared streamed reset")
							 .arg(ctxId)));
		resolvePendingStreamedReset(QStringLiteral("prepare"));
		return StreamResetPrepareResult::Ok;
	case StreamResetPrepareResult::Unsupported:
		error = QStringLiteral("that's unsupported by this session");
		break;
	case StreamResetPrepareResult::InvalidUser:
		error = QStringLiteral("isn't the one performing it");
		break;
	case StreamResetPrepareResult::InvalidMessageCount:
		clear = true;
		error = QStringLiteral("the actual message count is %1")
					.arg(history()->resetStreamMessageCount());
		break;
	case StreamResetPrepareResult::NotActive:
		error = QStringLiteral("none is active");
		break;
	case StreamResetPrepareResult::OutOfSpace:
		clear = true;
		error = QStringLiteral("the reset image is too large");
		break;
	case StreamResetPrepareResult::WriteError:
		clear = true;
		error = QStringLiteral("a write error occurred");
		break;
	case StreamResetPrepareResult::ConsumerError:
		clear = true;
		error = QStringLiteral("a stream consumer error occurred: %1")
					.arg(DP_error());
		break;
	}

	if(error.isEmpty()) {
		error = QStringLiteral("unknown error %1 occurred").arg(int(result));
	}

	log(Log()
			.about(Log::Level::Warn, Log::Topic::Status)
			.message(QStringLiteral("User %1 tried to finish stream reset with "
									"%2 messages, but %3")
						 .arg(ctxId)
						 .arg(expectedMessageCount)
						 .arg(error)));

	if(clear) {
		clearAutoReset(AUTORESET_FAILURE_RETRY_MSECS);
	}

	return result;
}

void ThinSession::resolvePendingStreamedReset(const QString &cause)
{
	SessionHistory *hist = history();
	if(checkStreamedResetStart(cause)) {
		size_t prevSizeInBytes = hist->sizeInBytes();
		size_t prevAutoResetThresholdBase = hist->autoResetThresholdBase();
		long long offset;
		QString error;
		if(hist->resolveStreamedReset(offset, error)) {
			Q_ASSERT(offset > 0);
			QLocale locale = QLocale::c();
			log(Log()
					.about(Log::Level::Info, Log::Topic::Status)
					.message(
						QStringLiteral("Resolved streamed reset after %1 with "
									   "offset %2 (size %3 => %4, autoreset "
									   "threshold base %5 => %6)")
							.arg(
								cause, QString::number(offset),
								locale.formattedDataSize(prevSizeInBytes),
								locale.formattedDataSize(hist->sizeInBytes()),
								locale.formattedDataSize(
									prevAutoResetThresholdBase),
								locale.formattedDataSize(
									hist->autoResetThresholdBase()))));
			for(Client *c : clients()) {
				ThinServerClient *tsc = static_cast<ThinServerClient *>(c);
				tsc->addToHistoryPosition(offset);
			}
			clearAutoReset();
			sendStatusUpdate();
			sendUpdatedSessionProperties();
		} else {
			log(Log()
					.about(Log::Level::Warn, Log::Topic::Status)
					.message(
						QStringLiteral("Error resolving streamed reset: %1")
							.arg(error)));
			clearAutoReset(AUTORESET_FAILURE_RETRY_MSECS);
		}
	}
}

void ThinSession::onSessionInitialized()
{
	clearAutoReset();
	sendStatusUpdate();
}

void ThinSession::onSessionReset()
{
	clearAutoReset();
	directToAll(net::ServerReply::makeCatchup(
		history()->lastIndex() - history()->firstIndex(), 0));
	sendStatusUpdate();
	history()->addMessage(net::ServerReply::makeCaughtUp(0));
}

void ThinSession::onClientJoin(Client *client, bool host)
{
	connect(
		history(), &SessionHistory::newMessagesAvailable,
		static_cast<ThinServerClient *>(client),
		&ThinServerClient::sendNextHistoryBatch);

	if(!host) {
		// Notify the client how many messages to expect (at least)
		// The client can use this information to display a progress bar during
		// the login phase
		int catchupKey = history()->nextCatchupKey();
		bool caughtUpAdded =
			history()->addMessage(net::ServerReply::makeCaughtUp(catchupKey));
		client->sendDirectMessage(net::ServerReply::makeCatchup(
			history()->lastIndex() - history()->firstIndex(),
			caughtUpAdded ? catchupKey : -1));
		client->sendDirectMessage(
			net::ServerReply::makeStatusUpdate(int(history()->sizeInBytes())));
	}
}

void ThinSession::onClientLeave(Client *client)
{
	Session::onClientLeave(client);
	uint8_t ctxId = client->id();
	invalidateAutoResetCandidate(ctxId);
	if(history()->abortStreamedReset(ctxId) == StreamResetAbortResult::Ok) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::Status)
				.message(
					QStringLiteral("User %1 left while streaming autoreset")
						.arg(ctxId)));
		clearAutoReset(AUTORESET_FAILURE_RETRY_MSECS);
	} else {
		resolvePendingStreamedReset(QStringLiteral("leave"));
	}
}

void ThinSession::onClientDeop(Client *client)
{
	uint8_t ctxId = client->id();
	invalidateAutoResetCandidate(ctxId);
	if(history()->abortStreamedReset(ctxId) == StreamResetAbortResult::Ok) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::Status)
				.message(
					QStringLiteral("User %1 deopped while streaming autoreset")
						.arg(ctxId)));
		client->sendDirectMessage(
			net::ServerReply::makeStreamedResetProgress(0, true));
		clearAutoReset(AUTORESET_FAILURE_RETRY_MSECS);
	}
}

void ThinSession::onResetStream(Client &client, const net::Message &msg)
{
	StreamResetAddResult result =
		history()->addStreamResetMessage(client.id(), msg);

	bool clear = false;
	QString error;
	switch(result) {
	case StreamResetAddResult::Ok:
		client.sendDirectMessage(
			net::ServerReply::makeStreamedResetProgress(0, false));
		return;
	case StreamResetAddResult::Unsupported:
		error = QStringLiteral("that's unsupported by this session");
		break;
	case StreamResetAddResult::InvalidUser:
		error = QStringLiteral("they're not the one tasked to do it");
		break;
	case StreamResetAddResult::BadType:
		clear = true;
		error = QStringLiteral("the message has bad type %1 (%2)")
					.arg(msg.type())
					.arg(msg.typeName());
		break;
	case StreamResetAddResult::DisallowedType:
		clear = true;
		error = QStringLiteral("of an illegal message in the reset image");
		break;
	case StreamResetAddResult::NotActive:
		error = QStringLiteral("no stream is active");
		break;
	case StreamResetAddResult::OutOfSpace:
		clear = true;
		error = QStringLiteral("the session is out of space");
		break;
	case StreamResetAddResult::WriteError:
		clear = true;
		error = QStringLiteral("a write error occurred");
		break;
	case StreamResetAddResult::ConsumerError:
		clear = true;
		error = QStringLiteral("a stream consumer error occurred: %1")
					.arg(DP_error());
		break;
	}

	if(error.isEmpty()) {
		error = QStringLiteral("unknown error %1 occurred").arg(int(result));
	}

	log(Log()
			.about(Log::Level::Warn, Log::Topic::Status)
			.message(QStringLiteral(
						 "User %1 tried to add a stream reset message, but %2")
						 .arg(client.id())
						 .arg(error)));
	client.sendDirectMessage(
		net::ServerReply::makeStreamedResetProgress(0, true));

	if(clear) {
		clearAutoReset(AUTORESET_FAILURE_RETRY_MSECS);
	}
}

void ThinSession::onStateChanged()
{
	// No matter what state we changed into, any autoreset dealings are moot.
	clearAutoReset();
}

void ThinSession::sendStatusUpdate()
{
	directToAll(
		net::ServerReply::makeStatusUpdate(int(history()->sizeInBytes())));
	resetLastStatusUpdate();
}

void ThinSession::checkAutoResetQuery()
{
	// Query for an autoreset only if the session is in a running state, we
	// haven't queried yet and we're not delaying after a failed autoreset.
	if(state() != State::Running ||
	   m_autoResetRequestStatus != AutoResetState::NotSent ||
	   !m_autoResetDelay.hasExpired()) {
		return;
	}

	// Only query for an autoreset if the threshold has been reached.
	size_t autoResetThreshold = history()->effectiveAutoResetThreshold();
	if(history()->sizeInBytes() <= autoResetThreshold) {
		return;
	}

	QLocale locale = QLocale::c();
	log(Log()
			.about(Log::Level::Info, Log::Topic::Status)
			.message(
				QString("Autoreset threshold (%1, effectively %2) reached.")
					.arg(locale.formattedDataSize(
						history()->autoResetThreshold()))
					.arg(locale.formattedDataSize(autoResetThreshold))));

	// Legacy alert for Drawpile 2.0.x versions
	directToAll(net::ServerReply::makeSizeLimitWarning(
		int(history()->sizeInBytes()), int(autoResetThreshold)));

	// New style for Drawpile 2.1.0 and newer
	// Autoreset request: send an autoreset query to each logged in
	// operator. The user that responds first gets to perform the reset.
	// Since Drawpile 2.2.2: also send a payload along to indicate extended
	// autoreset capabilities for candidate picking and streamed autoresets.
	m_autoResetPayload = generateAutoResetPayload();
	net::Message reqMsg = net::ServerReply::makeResetQuery(
		int(history()->sizeLimit()), m_autoResetPayload);

	for(Client *c : clients()) {
		if(c->isOperator() && !c->isGhost()) {
			c->setResetFlags(Client::ResetFlag::Queried);
			c->sendDirectMessage(reqMsg);
		} else {
			c->setResetFlags(Client::ResetFlag::None);
		}
	}

	m_autoResetCandidates.clear();
	m_autoResetRequestStatus = AutoResetState::Queried;
	m_autoResetTimer->start(AUTORESET_RESPONSE_DELAY_MSECS);
}

QString ThinSession::generateAutoResetPayload()
{
	static uint32_t autoResetIndex;
	return QStringLiteral("1:%1:%2")
		.arg(autoResetIndex++)
		.arg(QDateTime::currentMSecsSinceEpoch());
}

bool ThinSession::AutoResetCandidate::operator<(
	const AutoResetCandidate &other) const
{
	// Context id <= 0 means this is not a valid candidate, e.g. because they
	// lost op or they left outright.
	if(ctxId <= 0) {
		if(other.ctxId > 0) {
			return true;
		}
	} else if(other.ctxId <= 0) {
		return false;
	}

	// Clients that can do a streamed reset are preferable.
	if(!capabilities.testFlag(ResetCapability::GzipStream)) {
		if(other.capabilities.testFlag(ResetCapability::GzipStream)) {
			return true;
		}
	} else if(!other.capabilities.testFlag(ResetCapability::GzipStream)) {
		return false;
	}

	// Clients on a better operating system are preferable.
	if(osQuality < other.osQuality) {
		return true;
	} else if(osQuality > other.osQuality) {
		return false;
	}

	// Clients on a better network are preferable. At the time of writing, this
	// is only determined by the user manually toggling a setting for it.
	if(netQuality < other.netQuality) {
		return true;
	} else if(netQuality > other.netQuality) {
		return false;
	}

	// Clients with a lower average ping are preferable. Average pings <= 0.0
	// means that the client didn't give us any data, so we can't compare.
	if(averagePing > 0.0 && other.averagePing > 0.0) {
		if(averagePing > other.averagePing) {
			return true;
		} else if(averagePing < other.averagePing) {
			return false;
		}
	}

	// Finally, whoever responded first gets the cut.
	return other.responseRank < responseRank;
}

void ThinSession::triggerAutoReset()
{
	m_autoResetTimer->stop();

	// Queried means we asked for someone to reset the session, but nobody
	// answered. This can legitimately happen if there's no operators or if all
	// operators have their clients configured to not perform autoresets.
	if(m_autoResetRequestStatus == AutoResetState::Queried) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::Status)
				.message(QString(
					"Autoreset failed with no candidates to perform it")));
		clearAutoReset(AUTORESET_FAILURE_RETRY_MSECS);
		return;
	}

	if(m_autoResetRequestStatus != AutoResetState::QueriedWaiting) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::Status)
				.message(QString("Autoreset triggered unexpectedly in state %1")
							 .arg(int(m_autoResetRequestStatus))));
		return;
	}

	Client *c = nullptr;
	bool stream = false;
	std::sort(m_autoResetCandidates.begin(), m_autoResetCandidates.end());
	for(auto it = m_autoResetCandidates.rbegin(),
			 end = m_autoResetCandidates.rend();
		it != end; ++it) {
		int ctxId = it->ctxId;
		if(ctxId > 0) {
			Client *candidate = getClientById(ctxId);
			if(!candidate) {
				log(Log()
						.about(Log::Level::Warn, Log::Topic::Status)
						.message(
							QStringLiteral(
								"User %1 is gone, can't autoreset with them")
								.arg(ctxId)));
			} else if(!candidate->isOperator() || candidate->isGhost()) {
				log(Log()
						.about(Log::Level::Warn, Log::Topic::Status)
						.message(
							QStringLiteral("User %1 is not an operator, can't "
										   "autoreset with them")
								.arg(ctxId)));
			} else {
				c = candidate;
				stream = it->capabilities.testFlag(ResetCapability::GzipStream);
				break;
			}
		}
	}

	if(!c) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::Status)
				.message(QStringLiteral("Autoreset triggered, but no user "
										"available to execute it with")));
		clearAutoReset(AUTORESET_FAILURE_RETRY_MSECS);
		return;
	}

	m_autoResetRequestStatus = AutoResetState::Requested;
	m_autoResetCandidates.clear();

	if(stream) {
		c->setResetFlags(
			Client::ResetFlag::Awaiting | Client::ResetFlag::Streaming);
		c->sendDirectMessage(net::ServerReply::makeStreamedResetRequest(
			int(history()->sizeLimit()), m_autoResetPayload,
			QStringLiteral("gzip1")));
	} else {
		c->setResetFlags(Client::ResetFlag::Awaiting);
		c->sendDirectMessage(
			net::ServerReply::makeResetRequest(int(history()->sizeLimit())));
	}
}

void ThinSession::invalidateAutoResetCandidate(int ctxId)
{
	for(AutoResetCandidate &arc : m_autoResetCandidates) {
		if(arc.ctxId == ctxId) {
			arc.ctxId = -1;
		}
	}
}

void ThinSession::clearAutoReset(int retryDelay)
{
	if(m_autoResetTimer) {
		m_autoResetTimer->stop();
	}

	m_autoResetRequestStatus = AutoResetState::NotSent;
	for(Client *c : clients()) {
		c->setResetFlags(Client::ResetFlag::None);
	}

	if(history()->abortStreamedReset() == StreamResetAbortResult::Ok) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::Status)
				.message(QStringLiteral(
					"Streamed reset aborted by auto reset clear")));
	}

	m_autoResetDelay.setRemainingTime(retryDelay > 0 ? retryDelay : 0);
}

bool ThinSession::checkStreamedResetStart(const QString &cause)
{
	SessionHistory *hist = history();
	if(!hist->isResetStreamPending()) {
		return false;
	}

	long long resetStreamStartIndex = hist->resetStreamStartIndex();
	for(Client *c : clients()) {
		ThinServerClient *tsc = static_cast<ThinServerClient *>(c);
		long long historyPosition = tsc->historyPosition();
		if(historyPosition < resetStreamStartIndex) {
			if(m_lastAutoResetWarning.hasExpired()) {
				m_lastAutoResetWarning.setRemainingTime(
					AUTORESET_RESOLVE_LOG_MSECS);
				log(Log()
						.about(Log::Level::Warn, Log::Topic::Status)
						.message(
							QStringLiteral(
								"Streamed reset after %1 blocked by user %2 at "
								"position %3 < reset start %4")
								.arg(
									cause, QString::number(tsc->id()),
									QString::number(historyPosition),
									QString::number(historyPosition))));
			}
			return false;
		}
	}

	if(!hist->isStreamResetIoAvailable()) {
		if(m_lastAutoResetWarning.hasExpired()) {
			m_lastAutoResetWarning.setRemainingTime(
				AUTORESET_RESOLVE_LOG_MSECS);
			log(Log()
					.about(Log::Level::Warn, Log::Topic::Status)
					.message(
						QStringLiteral(
							"Streamed reset after %1 blocked by unavailable IO")
							.arg(cause)));
		}
		return false;
	}

	return true;
}

}
