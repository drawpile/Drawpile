// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/server/builtinsession.h"
#include "libclient/canvas/paintengine.h"
#include "libserver/client.h"
#include "libserver/inmemoryhistory.h"
#include "libserver/serverlog.h"
#include "libshared/net/servercmd.h"
#include "libshared/util/qtcompat.h"
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

void BuiltinSession::readyToAutoReset(int ctxId)
{
	log(Log()
			.about(Log::Level::Warn, Log::Topic::RuleBreak)
			.message(QStringLiteral(
						 "User %1 sent ready-to-autoreset to builtin server!")
						 .arg(ctxId)));
}

void BuiltinSession::doInternalReset(const drawdance::CanvasState &canvasState)
{
	m_softResetRequested = false;

	QVector<Client *> awaitingClients;
	for(Client *c : clients()) {
		if(c->isAwaitingReset()) {
			awaitingClients.append(c);
		}
	}

	if(awaitingClients.isEmpty()) {
		qWarning("No clients awaiting reset, not performing it");
		return;
	}

	internalReset(canvasState);

	auto [msgs, lastBatchIndex] = history()->getBatch(-1);
	// InMemoryHistory always returns the whole history
	Q_ASSERT(lastBatchIndex == history()->lastIndex());
	history()->reset(net::MessageList());

	net::Message catchup = net::ServerReply::makeCatchup(msgs.size());
	for(Client *c : awaitingClients) {
		c->setAwaitingReset(false);
		c->sendDirectMessage(catchup);
		c->sendDirectMessages(msgs);
	}
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
			client->sendDirectMessage(msg);
		}
	}
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

	net::Message catchup = net::ServerReply::makeCatchup(msgs.size());
	for(Client *client : clients()) {
		client->sendDirectMessage(catchup);
		client->sendDirectMessages(msgs);
	}
}

void BuiltinSession::onClientJoin(Client *client, bool host)
{
	if(host) {
		switchState(State::Running);
	} else {
		// The new client has to wait until the soft reset point is processed.
		// The paint engine will call us back once it has done so.
		client->setAwaitingReset(true);
		if(!m_softResetRequested) {
			directToAll(net::makeSoftResetMessage(
				m_paintEngine->aclState().localUserId()));
			m_softResetRequested = true;
		}
	}
}

void BuiltinSession::internalReset(const drawdance::CanvasState &canvasState)
{
	net::MessageList snapshot = serverSideStateMessages();
	uint8_t localUserId = m_paintEngine->aclState().localUserId();

	if(!m_pinnedMessage.isEmpty()) {
		snapshot.append(net::makeChatMessage(
			localUserId, 0, DP_MSG_CHAT_OFLAGS_PIN, m_pinnedMessage));
	}

	snapshot.append(
		net::makeUndoDepthMessage(0, m_paintEngine->undoDepthLimit()));

	canvasState.toResetImage(snapshot, 0);

	if(m_defaultLayer > 0) {
		snapshot.append(net::makeDefaultLayerMessage(0, m_defaultLayer));
	}

	m_acls.toResetImage(
		snapshot, localUserId, DP_ACL_STATE_RESET_IMAGE_SESSION_RESET_FLAGS);

	history()->reset(snapshot);

	double sizeInMiB = history()->sizeInBytes() / 1024.0 / 1024.0;
	log(Log()
			.about(Log::Level::Info, Log::Topic::Status)
			.message(
				QStringLiteral("Performed internal reset. Image size is %1 MB")
					.arg(sizeInMiB, 0, 'f', 2)));
}

}
