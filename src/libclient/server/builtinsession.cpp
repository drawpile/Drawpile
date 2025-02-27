// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/server/builtinsession.h"
#include "libclient/canvas/paintengine.h"
#include "libserver/client.h"
#include "libserver/inmemoryhistory.h"
#include "libserver/serverlog.h"
#include "libshared/net/servercmd.h"
#include "libshared/util/functionrunnable.h"
#include "libshared/util/qtcompat.h"
#include <QThreadPool>
#include <QVector>

namespace server {


BuiltinSession::BuiltinSession(
	ServerConfig *config, sessionlisting::Announcements *announcements,
	canvas::PaintEngine *paintEngine, const QString &id, const QString &idAlias,
	const QString &founder, QObject *parent)
	: Session(
		  new InMemoryHistory{
			  id, idAlias, protocol::ProtocolVersion::current(), founder},
		  config, announcements, parent)
	, m_paintEngine{paintEngine}
	, m_acls{m_paintEngine->aclState().clone(0)}
{
	history()->setParent(this);
}

bool BuiltinSession::supportsAutoReset() const
{
	return false;
}

void BuiltinSession::readyToAutoReset(
	const AutoResetResponseParams &params, const QString &payload)
{
	Q_UNUSED(payload);
	log(Log()
			.about(Log::Level::Warn, Log::Topic::RuleBreak)
			.message(QStringLiteral(
						 "User %1 sent ready-to-autoreset to builtin server!")
						 .arg(params.ctxId)));
}

void BuiltinSession::doInternalReset(const drawdance::CanvasState &canvasState)
{
	m_softResetRequested = false;
	if(m_reset) {
		qWarning("Internal reset triggered while one is already in progress");
		return;
	}

	if(!haveAnyClientAwaitingReset()) {
		qWarning("No clients awaiting reset, not performing it");
		return;
	}

	log(Log()
			.about(Log::Level::Debug, Log::Topic::Status)
			.message(QStringLiteral("Starting internal reset")));
	startInternalReset(canvasState);
}

StreamResetStartResult
BuiltinSession::handleStreamResetStart(int ctxId, const QString &correlator)
{
	Q_UNUSED(ctxId);
	Q_UNUSED(correlator);
	return StreamResetStartResult::Unsupported;
}

StreamResetAbortResult BuiltinSession::handleStreamResetAbort(int ctxId)
{
	Q_UNUSED(ctxId);
	return StreamResetAbortResult::Unsupported;
}

StreamResetPrepareResult
BuiltinSession::handleStreamResetFinish(int ctxId, int expectedMessageCount)
{
	Q_UNUSED(ctxId);
	Q_UNUSED(expectedMessageCount);
	return StreamResetPrepareResult::Unsupported;
}

void BuiltinSession::addToHistory(const net::Message &msg)
{
	if(state() == State::Shutdown) {
		return;
	}

	uint8_t result = m_acls.handle(msg);
	if(result & DP_ACL_STATE_FILTERED_BIT) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::RuleBreak)
				.message(QStringLiteral("ACL filtered %1 from %2")
							 .arg(msg.typeName())
							 .arg(msg.contextId())));
		return;
	}

	// Special messages
	DP_MessageType type = msg.type();
	if(type == DP_MSG_CHAT) {
		DP_MsgChat *mc = msg.toChat();
		if(DP_msg_chat_oflags(mc) & DP_MSG_CHAT_OFLAGS_PIN) {
			size_t len;
			const char *text = DP_msg_chat_message(mc, &len);
			if(len == 1 && *text == '-') {
				m_pinnedMessage.clear();
			} else {
				m_pinnedMessage =
					QString::fromUtf8(text, compat::castSize(len));
			}
		}
	} else if(type == DP_MSG_DEFAULT_LAYER) {
		m_defaultLayer = DP_msg_default_layer_id(msg.toDefaultLayer());
	}

	addedToHistory(msg);

	if(state() == State::Initialization) {
		// Send to everyone except the initializing user
		for(Client *client : clients()) {
			if(client->id() != initUserId()) {
				client->sendDirectMessage(msg);
			}
		}

	} else {
		for(Client *client : clients()) {
			if(!client->isAwaitingReset()) {
				client->sendDirectMessage(msg);
			}
		}
		if(m_reset) {
			m_reset->appendPostResetMessage(msg);
		}
	}
}

void BuiltinSession::onSessionInitialized()
{
	// Nothing to do.
}

void BuiltinSession::onSessionReset()
{
	auto [msgs, lastBatchIndex] = history()->getBatch(-1);
	// InMemoryHistory always returns the whole history
	Q_ASSERT(lastBatchIndex == history()->lastIndex());

	history()->reset(net::MessageList());

	m_acls.reset(0);
	for(const net::Message &msg : msgs) {
		m_acls.handle(msg);
	}

	net::Message catchup = net::ServerReply::makeCatchup(msgs.size(), 0);
	net::Message caughtup = net::ServerReply::makeCaughtUp(0);
	for(Client *client : clients()) {
		client->sendDirectMessage(catchup);
		client->sendDirectMessages(msgs);
		client->sendDirectMessage(caughtup);
	}
}

void BuiltinSession::onClientJoin(Client *client, bool host)
{
	if(host) {
		switchState(State::Running);
	} else {
		// The new client has to wait until the soft reset point is processed.
		// The paint engine will call us back once it has done so.
		client->setResetFlags(Client::ResetFlag::Awaiting);
		if(!m_softResetRequested && !m_reset) {
			directToAll(net::makeSoftResetMessage(
				m_paintEngine->aclState().localUserId()));
			m_softResetRequested = true;
		}
	}
}

void BuiltinSession::onClientDeop(Client *client)
{
	Q_UNUSED(client);
	// Nothing to do.
}

void BuiltinSession::onResetStream(Client &client, const net::Message &msg)
{
	Q_UNUSED(msg);
	log(Log()
			.about(Log::Level::Warn, Log::Topic::RuleBreak)
			.message(QStringLiteral("Client %1 sent reset stream message, but "
									"this is a builtin server")
						 .arg(client.id())));
}

void BuiltinSession::onStateChanged()
{
	// Nothing to do.
}

void BuiltinSession::chatMessageToAll(const net::Message &msg)
{
	Session::chatMessageToAll(msg);
	if(m_reset) {
		m_reset->appendPostResetMessage(msg);
	}
}

bool BuiltinSession::haveAnyClientAwaitingReset() const
{
	for(const Client *c : clients()) {
		if(c->isAwaitingReset()) {
			return true;
		}
	}
	return false;
}

void BuiltinSession::startInternalReset(
	const drawdance::CanvasState &canvasState)
{
	m_reset.reset(new BuiltinReset(serverSideStateMessages()));
	uint8_t localUserId = m_paintEngine->aclState().localUserId();

	if(!m_pinnedMessage.isEmpty()) {
		m_reset->appendResetImageMessage(net::makeChatMessage(
			localUserId, 0, DP_MSG_CHAT_OFLAGS_PIN, m_pinnedMessage));
	}

	m_reset->appendResetImageMessage(
		net::makeUndoDepthMessage(0, m_paintEngine->undoDepthLimit()));

	if(m_defaultLayer > 0) {
		m_reset->appendPostResetMessage(
			net::makeDefaultLayerMessage(0, m_defaultLayer));
	}

	m_reset->appendAclsToPostResetMessages(m_acls, localUserId);

	utils::FunctionRunnable *runnable =
		new utils::FunctionRunnable([reset = m_reset, canvasState]() {
			reset->generateResetImage(canvasState);
		});
	connect(
		m_reset.get(), &BuiltinReset::resetImageGenerated, this,
		&BuiltinSession::finishInternalReset, Qt::QueuedConnection);
	QThreadPool::globalInstance()->start(runnable);
}

void BuiltinSession::finishInternalReset()
{
	history()->reset(m_reset->takeResetImage());

	double sizeInMiB = history()->sizeInBytes() / 1024.0 / 1024.0;
	log(Log()
			.about(Log::Level::Info, Log::Topic::Status)
			.message(
				QStringLiteral("Performed internal reset. Image size is %1 MB")
					.arg(sizeInMiB, 0, 'f', 2)));

	auto [msgs, lastBatchIndex] = history()->getBatch(-1);
	// InMemoryHistory always returns the whole history
	Q_ASSERT(lastBatchIndex == history()->lastIndex());
	history()->reset(net::MessageList());

	net::Message catchup = net::ServerReply::makeCatchup(msgs.size(), 1);
	net::Message caughtup = net::ServerReply::makeCaughtUp(1);
	for(Client *c : clients()) {
		if(c->isAwaitingReset()) {
			c->setResetFlags(Client::ResetFlag::None);
			c->sendDirectMessage(catchup);
			c->sendDirectMessages(msgs);
			c->sendDirectMessage(caughtup);
		}
	}

	m_reset.clear();
	sendUpdatedSessionProperties();
	sendUpdatedAnnouncementList();
	sendUpdatedBanlist();
	sendUpdatedMuteList();
	sendUpdatedAuthList();
	sendUpdatedInviteList();
}

}
