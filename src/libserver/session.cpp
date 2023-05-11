// SPDX-License-Identifier: GPL-3.0-or-later

#include "libserver/session.h"
#include "libserver/client.h"
#include "libserver/serverconfig.h"
#include "libserver/serverlog.h"
#include "libserver/opcommands.h"
#include "libserver/announcements.h"

#include "libshared/net/control.h"
#include "libshared/net/meta.h"
#include "libshared/record/writer.h"
#include "libshared/util/filename.h"
#include "libshared/util/passwordhash.h"
#include "libshared/util/networkaccess.h"

#include <QTimer>
#include <QNetworkRequest>
#include <QNetworkReply>

namespace server {

using protocol::MessagePtr;

static bool forceEnableNsfm(SessionHistory *history, const ServerConfig *config)
{
	if(config->getConfigBool(config::ForceNsfm) && !history->hasFlag(SessionHistory::Nsfm)) {
		SessionHistory::Flags flags = history->flags();
		flags.setFlag(SessionHistory::Nsfm);
		history->setFlags(flags);
		return true;
	} else {
		return false;
	}
}

Session::Session(SessionHistory *history, ServerConfig *config, sessionlisting::Announcements *announcements, QObject *parent)
	: QObject(parent),
	m_history(history),
	m_config(config),
	m_announcements(announcements)
{
	m_history->setParent(this);
	connect(m_config, &ServerConfig::configValueChanged, this, &Session::onConfigValueChanged);
	forceEnableNsfm(m_history, m_config);

	m_lastEventTime.start();

	// History already exists? Skip the Initialization state.
	if(history->sizeInBytes()>0)
		m_state = State::Running;

	// Session announcements
	connect(m_announcements, &sessionlisting::Announcements::announcementsChanged, this, &Session::onAnnouncementsChanged);
	connect(m_announcements, &sessionlisting::Announcements::announcementError, this, &Session::onAnnouncementError);
	for(const QString &announcement : m_history->announcements())
		makeAnnouncement(QUrl(announcement), false);
}

static protocol::MessagePtr makeLogMessage(const Log &log)
{
	protocol::ServerReply sr {
		protocol::ServerReply::LOG,
		log.message(),
		log.toJson(Log::NoPrivateData|Log::NoSession)
	};
	return protocol::MessagePtr(new protocol::Command(0, sr));
}

protocol::MessageList Session::serverSideStateMessages() const
{
	protocol::MessageList msgs;

	QList<uint8_t> owners;
	QList<uint8_t> trusted;

	for(const Client *c : m_clients) {
		msgs << c->joinMessage();
		if(c->isOperator())
			owners << c->id();
		if(c->isTrusted())
			trusted << c->id();
	}

	msgs << protocol::MessagePtr(new protocol::SessionOwner(0, owners));

	if(!trusted.isEmpty())
		msgs << protocol::MessagePtr(new protocol::TrustedUsers(0, trusted));

	return msgs;
}

void Session::switchState(State newstate)
{
	if(newstate==State::Initialization) {
		qFatal("Illegal state change to Initialization from %d", int(m_state));

	} else if(newstate==State::Running) {
		if(m_state!=State::Initialization && m_state!=State::Reset)
			qFatal("Illegal state change to Running from %d", int(m_state));

		m_initUser = -1;
		bool success = true;

		if(m_state==State::Reset && !m_resetstream.isEmpty()) {
			// Reset buffer uploaded. Now perform the reset before returning to
			// normal running state.

			auto resetImage = serverSideStateMessages() + m_resetstream;

			// Send reset snapshot
			if(!m_history->reset(resetImage)) {
				// This shouldn't normally happen, as the size limit should be caught while
				// still uploading the reset.
				messageAll("Session reset failed!", true);
				success = false;

			} else {
				protocol::ServerReply resetcmd;
				resetcmd.type = protocol::ServerReply::RESET;
				resetcmd.reply["state"] = "reset";
				resetcmd.message = "Session reset!";
				directToAll(MessagePtr(new protocol::Command(0, resetcmd)));

				onSessionReset();

				sendUpdatedSessionProperties();
			}

			m_resetstream = protocol::MessageList();
			m_resetstreamsize = 0;
		}

		if(success && !m_recordingFile.isEmpty())
			restartRecording();

		for(Client *c : m_clients)
			c->setHoldLocked(false);

	} else if(newstate==State::Reset) {
		if(m_state!=State::Running)
			qFatal("Illegal state change to Reset from %d", int(m_state));

		m_resetstream.clear();
		m_resetstreamsize = 0;
		messageAll("Preparing for session reset!", true);
	}

	m_state = newstate;
}

void Session::assignId(Client *user)
{
	uint8_t id = m_history->idQueue().getIdForName(user->username());

	int loops=256;
	while(loops>0 && (id==0 || getClientById(id))) {
		id = m_history->idQueue().nextId();
		  --loops;
	}
	Q_ASSERT(loops>0); // shouldn't happen, since we don't let new users in if the session is full
	user->setId(id);
}

void Session::joinUser(Client *user, bool host)
{
	user->setSession(this);
	m_clients.append(user);

	connect(user, &Client::loggedOff, this, &Session::removeUser);

	m_pastClients.remove(user->id());

	// Send session log history to the new client
	{
		QList<Log> log = m_config->logger()->query().session(id()).atleast(Log::Level::Info).get();
		// Note: the query returns the log entries in latest first, but we send
		// new entries to clients as they occur, so we reverse the list before sending it
		for(int i=log.size()-1;i>=0;--i) {
			user->sendDirectMessage(makeLogMessage(log.at(i)));
		}
	}

	if(host) {
		Q_ASSERT(m_state == State::Initialization);
		m_initUser = user->id();

	} else {
		if(m_state == State::Initialization)
			user->setHoldLocked(true);
	}

	onClientJoin(user, host);

	addToHistory(user->joinMessage());

	if(user->isOperator() || m_history->isOperator(user->authId()))
		changeOpStatus(user->id(), true, "the server");

	if(user->authFlags().contains("TRUSTED") || m_history->isTrusted(user->authId()))
		changeTrustedStatus(user->id(), true, "the server");

	ensureOperatorExists();

	const QString welcomeMessage = m_config->getConfigString(config::WelcomeMessage);
	if(!welcomeMessage.isEmpty()) {
		user->sendSystemChat(welcomeMessage);
	}

	// Make sure everyone is up to date
	sendUpdatedAnnouncementList();
	sendUpdatedBanlist();
	sendUpdatedMuteList();

	m_history->joinUser(user->id(), user->username());

	user->log(Log().about(Log::Level::Info, Log::Topic::Join).message("Joined session"));
	emit sessionAttributeChanged(this);
}

void Session::removeUser(Client *user)
{
	if(!m_clients.removeOne(user))
		return;

	m_pastClients.insert(user->id(), PastClient {
		user->id(),
		user->authId(),
		user->username(),
		user->peerAddress(),
		!user->isModerator()
	});

	Q_ASSERT(user->session() == this);
	user->log(Log().about(Log::Level::Info, Log::Topic::Leave).message("Left session"));
	user->setSession(nullptr);

	disconnect(user, nullptr, this, nullptr);
	disconnect(m_history, nullptr, user, nullptr);

	if(user->id() == m_initUser && m_state == State::Reset) {
		// Whoops, the resetter left before the job was done!
		// We simply cancel the reset in that case and go on
		abortReset();
	}

	addToHistory(MessagePtr(new protocol::UserLeave(user->id())));
	m_history->idQueue().reserveId(user->id()); // Try not to reuse the ID right away

	ensureOperatorExists();

	// Reopen the session when the last user leaves
	if(m_clients.isEmpty()) {
		setClosed(false);
	}

	emit sessionAttributeChanged(this);
}

void Session::abortReset()
{
	m_initUser = -1;
	m_resetstream.clear();
	m_resetstreamsize = 0;
	switchState(State::Running);
	messageAll("Session reset cancelled.", true);
}

Client *Session::getClientById(uint8_t id)
{
	for(Client *c : m_clients) {
		if(c->id() == id)
			return c;
	}
	return nullptr;
}

Client *Session::getClientByUsername(const QString &username)
{
	for(Client *c : m_clients) {
		if(c->username().compare(username, Qt::CaseInsensitive)==0)
			return c;
	}
	return nullptr;
}

void Session::addBan(const Client *target, const QString &bannedBy)
{
	Q_ASSERT(target);
	if(m_history->addBan(target->username(), target->peerAddress(), target->authId(), bannedBy)) {
		target->log(Log().about(Log::Level::Info, Log::Topic::Ban).message("Banned by " + bannedBy));
		sendUpdatedBanlist();
	}
}

void Session::addBan(const PastClient &target, const QString &bannedBy)
{
	Q_ASSERT(target.id>0);
	if(m_history->addBan(target.username, target.peerAddress, target.authId, bannedBy)) {
		log(Log().user(target.id, target.peerAddress, target.username).about(Log::Level::Info, Log::Topic::Ban).message("Banned by " + bannedBy));
		sendUpdatedBanlist();
	}
}

void Session::removeBan(int entryId, const QString &removedBy)
{
	QString unbanned = m_history->removeBan(entryId);
	if(!unbanned.isEmpty()) {
		log(Log().about(Log::Level::Info, Log::Topic::Unban).message(unbanned + " unbanned by " + removedBy));
		sendUpdatedBanlist();
	}
}

bool Session::isClosed() const
{
	return m_closed
		|| userCount() >= m_history->maxUsers()
		|| (m_state != State::Initialization && m_state != State::Running);
}

void Session::setClosed(bool closed)
{
	if(m_closed != closed) {
		m_closed = closed;
		sendUpdatedSessionProperties();
	}
}

void Session::setSessionConfig(const QJsonObject &conf, Client *changedBy)
{
	QStringList changes;

	if(conf.contains("closed")) {
		m_closed = conf["closed"].toBool();
		changes << (m_closed ? "closed" : "opened");
	}

	SessionHistory::Flags flags = m_history->flags();

	if(conf.contains("authOnly")) {
		const bool authOnly = conf["authOnly"].toBool();
		// The authOnly flag can only be set by an authenticated user.
		// Otherwise it would be possible for users to accidentally lock themselves out.
		if(!authOnly || !changedBy || changedBy->isAuthenticated()) {
			flags.setFlag(SessionHistory::AuthOnly, authOnly);
			changes << (authOnly ? "blocked guest logins" : "permitted guest logins");
		}
	}

	if(conf.contains("persistent")) {
		flags.setFlag(SessionHistory::Persistent, conf["persistent"].toBool() && m_config->getConfigBool(config::EnablePersistence));
		changes << (conf["persistent"].toBool() ? "made persistent" : "made nonpersistent");
	}

	if(conf.contains("title")) {
		m_history->setTitle(conf["title"].toString().mid(0, 100));
		changes << "changed title";
	}

	if(conf.contains("maxUserCount")) {
		m_history->setMaxUsers(conf["maxUserCount"].toInt());
		changes << "changed max. user count";
	}

	if(conf.contains("resetThreshold")) {
		int val;
		if(conf["resetThreshold"].isDouble())
			val = conf["resetThreshold"].toInt();
		else
			val = ServerConfig::parseSizeString(conf["resetThreshold"].toString());
		m_history->setAutoResetThreshold(val);
		changes << "changed autoreset threshold";
	}

	if(conf.contains("password")) {
		m_history->setPassword(conf["password"].toString());
		changes << "changed password";
	}

	if(conf.contains("opword")) {
		m_history->setOpword(conf["opword"].toString());
		changes << "changed opword";
	}

	// Note: this bit is only relayed by the server: it informs
	// the client whether to send preserved/recorded chat messages
	// by default.
	if(conf.contains("preserveChat")) {
		flags.setFlag(SessionHistory::PreserveChat, conf["preserveChat"].toBool());
		changes << (conf["preserveChat"].toBool() ? "preserve chat" : "don't preserve chat");
	}

	if(m_config->getConfigBool(config::ForceNsfm)) {
		if(!flags.testFlag(SessionHistory::Nsfm)) {
			flags.setFlag(SessionHistory::Nsfm, true);
			changes << "forced NSFM";
		}
	} else if(conf.contains("nsfm")) {
		flags.setFlag(SessionHistory::Nsfm, conf["nsfm"].toBool());
		changes << (conf["nsfm"].toBool() ? "tagged NSFM" : "removed NSFM tag");
	}

	if(conf.contains("deputies")) {
		flags.setFlag(SessionHistory::Deputies, conf["deputies"].toBool());
		changes << (conf["deputies"].toBool() ? "enabled deputies" : "disabled deputies");
	}

	m_history->setFlags(flags);

	if(!changes.isEmpty()) {
		sendUpdatedSessionProperties();
		QString logmsg = changes.join(", ");
		logmsg[0] = logmsg[0].toUpper();

		Log l = Log().about(Log::Level::Info, Log::Topic::Status).message(logmsg);
		if(changedBy)
			changedBy->log(l);
		else
			log(l);
	}
}

QList<uint8_t> Session::updateOwnership(QList<uint8_t> ids, const QString &changedBy)
{
	QList<uint8_t> truelist;
	Client *kickResetter = nullptr;
	for(Client *c : m_clients) {
		const bool op = ids.contains(c->id()) || c->isModerator();
		if(op != c->isOperator()) {
			if(!op && c->id() == m_initUser && m_state == State::Reset) {
				// OP status removed mid-reset! The user probably has at least part
				// of the reset image still queued for upload, which will messs up
				// the session once we're out of reset mode. Kicking the client
				// is the easiest workaround.
				// TODO for 3.0: send a cancel command to the client and ignore
				// all further input until ack is received.
				kickResetter = c;
			}

			c->setOperator(op);
			QString msg;
			if(op) {
				msg = "Made operator by " + changedBy;
				c->log(Log().about(Log::Level::Info, Log::Topic::Op).message(msg));
			} else {
				msg = "Operator status revoked by " + changedBy;
				c->log(Log().about(Log::Level::Info, Log::Topic::Deop).message(msg));
			}
			messageAll(c->username() + " " + msg, false);
			if(c->isAuthenticated() && !c->isModerator())
				m_history->setAuthenticatedOperator(c->authId(), op);

		}
		if(c->isOperator())
			truelist << c->id();
	}

	if(kickResetter)
		kickResetter->disconnectClient(Client::DisconnectionReason::Error, "De-opped while resetting");

	return truelist;
}

void Session::changeOpStatus(uint8_t id, bool op, const QString &changedBy)
{
	QList<uint8_t> ids;
	for(const Client *c : m_clients) {
		if(c->isOperator())
			ids << c->id();
	}

	if(op)
		ids << id;
	else
		ids.removeOne(id);

	ids = updateOwnership(ids, changedBy);
	addToHistory(protocol::MessagePtr(new protocol::SessionOwner(0, ids)));
}

QList<uint8_t> Session::updateTrustedUsers(QList<uint8_t> ids, const QString &changedBy)
{
	QList<uint8_t> truelist;
	for(Client *c : m_clients) {
		const bool trusted = ids.contains(c->id());
		if(trusted != c->isTrusted()) {
			c->setTrusted(trusted);
			QString msg;
			if(trusted) {
				msg = "Trusted by " + changedBy;
				c->log(Log().about(Log::Level::Info, Log::Topic::Trust).message(msg));
			} else {
				msg = "Untrusted by " + changedBy;
				c->log(Log().about(Log::Level::Info, Log::Topic::Untrust).message(msg));
			}
			messageAll(c->username() + " " + msg, false);
			if(c->isAuthenticated())
				m_history->setAuthenticatedTrust(c->authId(), trusted);

		}
		if(c->isTrusted())
			truelist << c->id();
	}

	return truelist;
}

void Session::changeTrustedStatus(uint8_t id, bool trusted, const QString &changedBy)
{
	QList<uint8_t> ids;
	for(const Client *c : m_clients) {
		if(c->isTrusted())
			ids << c->id();
	}

	if(trusted)
		ids << id;
	else
		ids.removeOne(id);

	ids = updateTrustedUsers(ids, changedBy);
	addToHistory(protocol::MessagePtr(new protocol::TrustedUsers(0, ids)));
}

void Session::sendUpdatedSessionProperties()
{
	protocol::ServerReply props;
	props.type = protocol::ServerReply::SESSIONCONF;
	QJsonObject	conf;
	conf["closed"] = m_closed; // this refers specifically to the closed flag, not the general status
	conf["authOnly"] = m_history->hasFlag(SessionHistory::AuthOnly);
	conf["persistent"] = m_history->hasFlag(SessionHistory::Persistent);
	conf["title"] = m_history->title();
	conf["maxUserCount"] = m_history->maxUsers();
	conf["resetThreshold"] = int(m_history->autoResetThreshold());
	conf["resetThresholdBase"] = int(m_history->autoResetThresholdBase());
	conf["preserveChat"] = m_history->flags().testFlag(SessionHistory::PreserveChat);
	conf["nsfm"] = m_history->flags().testFlag(SessionHistory::Nsfm);
	conf["deputies"] = m_history->flags().testFlag(SessionHistory::Deputies);
	conf["hasPassword"] = !m_history->passwordHash().isEmpty();
	conf["hasOpword"] = !m_history->opwordHash().isEmpty();
	// This config option is basically a session property set by the server.
	// We report it here so the client can disable the checkbox in the UI.
	conf["forceNsfm"] = m_config->getConfigBool(config::ForceNsfm);
	props.reply["config"] = conf;

	addToHistory(protocol::MessagePtr(new protocol::Command(0, props)));
	emit sessionAttributeChanged(this);
}

void Session::sendUpdatedBanlist()
{
	// The banlist is not usually included in the sessionconf.
	// Moderators and local users get to see the actual IP addresses too
	protocol::ServerReply msg;
	msg.type = protocol::ServerReply::SESSIONCONF;
	QJsonObject conf;
	conf["banlist"] = m_history->banlist().toJson(false);
	msg.reply["config"] = conf;

	// Normal users don't get to see the actual IP addresses
	const protocol::MessagePtr normalVersion(new protocol::Command(0, msg));

	// But moderators and local users do
	conf["banlist"] = m_history->banlist().toJson(true);
	msg.reply["config"] = conf;
	const protocol::MessagePtr modVersion(new protocol::Command(0, msg));

	for(Client *c : m_clients) {
		if(c->isModerator() || c->peerAddress().isLoopback())
			c->sendDirectMessage(modVersion);
		else
			c->sendDirectMessage(normalVersion);
	}
}

void Session::sendUpdatedAnnouncementList()
{
	// The announcement list is not usually included in the sessionconf.
	protocol::ServerReply msg;
	msg.type = protocol::ServerReply::SESSIONCONF;
	QJsonArray list;
	const auto announcements = m_announcements->getAnnouncements(this);
	for(const sessionlisting::Announcement &a : announcements) {
		QJsonObject o;
		o["url"] = a.apiUrl.toString();
		o["roomcode"] = a.roomcode;
		o["private"] = a.isPrivate;
		list.append(o);
	}

	QJsonObject conf;
	conf["announcements"]= list;
	msg.reply["config"] = conf;
	directToAll(protocol::MessagePtr(new protocol::Command(0, msg)));
}

void Session::sendUpdatedMuteList()
{
	// The mute list is not usually included in the sessionconf.
	protocol::ServerReply msg;
	msg.type = protocol::ServerReply::SESSIONCONF;
	QJsonArray muted;
	for(const Client *c : m_clients) {
		if(c->isMuted())
			muted.append(c->id());
	}

	QJsonObject conf;
	conf["muted"]= muted;
	msg.reply["config"] = conf;
	directToAll(protocol::MessagePtr(new protocol::Command(0, msg)));
}

void Session::handleClientMessage(Client &client, protocol::MessagePtr msg)
{
	// Filter away server-to-client-only messages
	switch(msg->type()) {
	using namespace protocol;
	case MSG_USER_JOIN:
	case MSG_USER_LEAVE:
	case MSG_SOFTRESET:
		client.log(Log().about(Log::Level::Warn, Log::Topic::RuleBreak).message("Received server-to-user only command " + msg->messageName()));
		return;
	case MSG_DISCONNECT:
		// we don't do anything with disconnect notifications from the client
		return;
	default: break;
	}

	// Some meta commands affect the server too
	switch(msg->type()) {
		case protocol::MSG_COMMAND: {
			protocol::ServerCommand cmd = msg.cast<protocol::Command>().cmd();
			handleClientServerCommand(&client, cmd.cmd, cmd.args, cmd.kwargs);
			return;
		}
		case protocol::MSG_SESSION_OWNER: {
			if(!client.isOperator()) {
				client.log(Log().about(Log::Level::Warn, Log::Topic::RuleBreak).message("Tried to change session ownership"));
				return;
			}

			QList<uint8_t> ids = msg.cast<protocol::SessionOwner>().ids();
			ids.append(client.id());
			ids = updateOwnership(ids, client.username());
			msg.cast<protocol::SessionOwner>().setIds(ids);
			break;
		}
		case protocol::MSG_CHAT: {
			if(client.isMuted())
				return;
			if(msg.cast<protocol::Chat>().isBypass()) {
				directToAll(msg);
				return;
			}
			break;
		}
		case protocol::MSG_PRIVATE_CHAT: {
			const protocol::PrivateChat &chat = msg.cast<protocol::PrivateChat>();
			if(chat.target()>0) {
				Client *target = getClientById(chat.target());
				if(target) {
					client.sendDirectMessage(msg);
					target->sendDirectMessage(msg);
				}
			}
			return;
		}
		case protocol::MSG_TRUSTED_USERS: {
			if(!client.isOperator()) {
				log(Log().about(Log::Level::Warn, Log::Topic::RuleBreak).message("Tried to change trusted user list"));
				return;
			}

			QList<uint8_t> ids = msg.cast<protocol::TrustedUsers>().ids();
			ids = updateTrustedUsers(ids, client.username());
			msg.cast<protocol::TrustedUsers>().setIds(ids);
			break;
		}
		default: break;
	}

	// Rest of the messages are added to session history
	if(initUserId() == client.id())
		addToInitStream(msg);
	else
		addToHistory(msg);
}

void Session::addToInitStream(protocol::MessagePtr msg)
{
	Q_ASSERT(m_state != State::Running);

	if(m_state == State::Initialization) {
		addToHistory(msg);

	} else if(m_state == State::Reset) {
		m_resetstreamsize += msg->length();
		m_resetstream.append(msg);

		// Well behaved clients should be aware of the history limit and not exceed it.
		if(m_history->sizeLimit()>0 && m_resetstreamsize > m_history->sizeLimit()) {
			Client *resetter = getClientById(m_initUser);
			if(resetter)
				resetter->disconnectClient(Client::DisconnectionReason::Error, "History limit exceeded");
		}
	}
}

void Session::handleInitBegin(int ctxId)
{
	Client *c = getClientById(ctxId);
	if(!c) {
		// Shouldn't happen
		log(Log().about(Log::Level::Error, Log::Topic::RuleBreak).message(QString("Non-existent user %1 sent init-begin").arg(ctxId)));
		return;
	}

	if(ctxId != m_initUser) {
		c->log(Log().about(Log::Level::Warn, Log::Topic::RuleBreak).message(QString("Sent init-begin, but init user is #%1").arg(m_initUser)));
		return;
	}

	c->log(Log().about(Log::Level::Debug, Log::Topic::Status).message("init-begin"));

	// It's possible that regular non-reset commands were still in the upload buffer
	// when the client started sending the reset snapshot. The init-begin indicates
	// the start of the true reset snapshot, so we can clear out the buffer here.
	// For backward-compatibility, sending the init-begin command is optional.
	if(m_resetstreamsize>0) {
		c->log(Log().about(Log::Level::Debug, Log::Topic::Status).message(QStringLiteral("%1 extra messages cleared by init-begin").arg(m_resetstream.size())));
		m_resetstream.clear();
		m_resetstreamsize = 0;
	}
}

void Session::handleInitComplete(int ctxId)
{
	Client *c = getClientById(ctxId);
	if(!c) {
		// Shouldn't happen
		log(Log().about(Log::Level::Error, Log::Topic::RuleBreak).message(QString("Non-existent user %1 sent init-complete").arg(ctxId)));
		return;
	}

	if(ctxId != m_initUser) {
		c->log(Log().about(Log::Level::Warn, Log::Topic::RuleBreak).message(QString("Sent init-complete, but init user is #%1").arg(m_initUser)));
		return;
	}

	c->log(Log().about(Log::Level::Debug, Log::Topic::Status).message("init-complete"));

	switchState(State::Running);
}


void Session::handleInitCancel(int ctxId)
{
	Client *c = getClientById(ctxId);
	if(!c) {
		// Shouldn't happen
		log(Log().about(Log::Level::Error, Log::Topic::RuleBreak).message(QString("Non-existent user %1 sent init-complete").arg(ctxId)));
		return;
	}

	if(ctxId != m_initUser) {
		c->log(Log().about(Log::Level::Warn, Log::Topic::RuleBreak).message(QString("Sent init-cancel, but init user is #%1").arg(m_initUser)));
		return;
	}

	c->log(Log().about(Log::Level::Debug, Log::Topic::Status).message("init-cancel"));
	abortReset();
}

void Session::resetSession(int resetter)
{
	Q_ASSERT(m_state == State::Running);
	Q_ASSERT(getClientById(resetter));

	m_initUser = resetter;
	switchState(State::Reset);

	protocol::ServerReply resetRequest;
	resetRequest.type = protocol::ServerReply::RESET;
	resetRequest.reply["state"] = "init";
	resetRequest.message = "Prepared to receive session data";

	getClientById(resetter)->sendDirectMessage(protocol::MessagePtr(new protocol::Command(0, resetRequest)));
}

void Session::killSession(bool terminate)
{
	if(m_state == State::Shutdown)
		return;

	switchState(State::Shutdown);
	unlistAnnouncement(QUrl(), false);
	stopRecording();

	for(Client *c : m_clients) {
		c->disconnectClient(Client::DisconnectionReason::Shutdown, "Session terminated");
		c->setSession(nullptr);
	}
	m_clients.clear();

	if(terminate)
		m_history->terminate();

	this->deleteLater();
}

void Session::directToAll(protocol::MessagePtr msg)
{
	for(Client *c : m_clients) {
		c->sendDirectMessage(msg);
	}
}

void Session::messageAll(const QString &message, bool alert)
{
	if(message.isEmpty())
		return;

	directToAll(protocol::MessagePtr(new protocol::Command(0,
		(protocol::ServerReply {
			alert ? protocol::ServerReply::ALERT : protocol::ServerReply::MESSAGE,
			message,
			QJsonObject()
		}).toJson()))
	);
}

void Session::ensureOperatorExists()
{
	// If there is a way to gain OP status without being explicitly granted,
	// it's OK for the session to not have any operators for a while.
	if(!m_history->opwordHash().isEmpty() || m_history->isAuthenticatedOperators())
		return;

	bool hasOp=false;
	for(const Client *c : m_clients) {
		if(c->isOperator()) {
			hasOp=true;
			break;
		}
	}

	if(!hasOp && !m_clients.isEmpty()) {
		changeOpStatus(m_clients.first()->id(), true, "the server");
	}
}

void Session::addedToHistory(protocol::MessagePtr msg)
{
	if(m_recorder)
		m_recorder->recordMessage(msg);

	m_lastEventTime.start();
	// TODO calculate activity score that can be shown in listings
}

void Session::restartRecording()
{
	if(m_recorder) {
		m_recorder->close();
		delete m_recorder;
	}

	// Start recording
	QString filename = utils::makeFilenameUnique(m_recordingFile, ".dprec");
	qDebug("Starting session recording %s", qPrintable(filename));

	m_recorder = new recording::Writer(filename, this);
	if(!m_recorder->open()) {
		qWarning("Couldn't write session recording to %s: %s", qPrintable(filename), qPrintable(m_recorder->errorString()));
		delete m_recorder;
		m_recorder = nullptr;
		return;
	}

	QJsonObject metadata;
	metadata["server-recording"] = true;
	metadata["version"] = m_history->protocolVersion().asString();

	m_recorder->writeHeader(metadata);
	m_recorder->setAutoflush();

	int lastBatchIndex=0;
	do {
		protocol::MessageList history;
		std::tie(history, lastBatchIndex) = m_history->getBatch(lastBatchIndex);
		for(MessagePtr m : history)
			m_recorder->recordMessage(m);

	} while(lastBatchIndex<m_history->lastIndex());
}

void Session::stopRecording()
{
	if(m_recorder) {
		m_recorder->close();
		delete m_recorder;
		m_recorder = nullptr;
	}
}

QString Session::uptime() const
{
	qint64 up = (QDateTime::currentMSecsSinceEpoch() - m_history->startTime().toMSecsSinceEpoch()) / 1000;

	int days = up / (60*60*24);
	up -= days * (60*60*24);

	int hours = up / (60*60);
	up -= hours * (60*60);

	int minutes = up / 60;

	QString uptime;
	if(days==1)
		uptime = "one day, ";
	else if(days>1)
		uptime = QString::number(days) + " days, ";

	if(hours==1)
		uptime += "1 hour and ";
	else
		uptime += QString::number(hours) + " hours and ";

	if(minutes==1)
		uptime += "1 minute";
	else
		uptime += QString::number(minutes) + " minutes.";

	return uptime;
}

QStringList Session::userNames() const
{
	QStringList lst;
	for(const Client *c : m_clients)
		lst << c->username();
	return lst;
}

void Session::makeAnnouncement(const QUrl &url, bool privateListing)
{
	Q_ASSERT(m_announcements);
	m_announcements->announceSession(this, url, privateListing ? sessionlisting::PrivacyMode::Private : sessionlisting::PrivacyMode::Public);
}

void Session::unlistAnnouncement(const QUrl &url, bool terminate)
{
	Q_ASSERT(m_announcements);
	m_announcements->unlistSession(this, url);

	if(terminate)
		m_history->removeAnnouncement(url.toString());
}

sessionlisting::Session Session::getSessionAnnouncement() const
{
	const bool privateUserList = m_config->getConfigBool(config::PrivateUserList);

	return sessionlisting::Session {
		m_config->internalConfig().localHostname,
		m_config->internalConfig().getAnnouncePort(),
		aliasOrId(),
		m_history->protocolVersion(),
		m_history->title(),
		userCount(),
		(!m_history->passwordHash().isEmpty() || privateUserList) ? QStringList() : userNames(),
		!m_history->passwordHash().isEmpty(),
		m_history->hasFlag(SessionHistory::Nsfm),
		sessionlisting::PrivacyMode::Undefined,
		m_history->founderName(),
		m_history->startTime()
	};
}


void Session::onAnnouncementsChanged(const sessionlisting::Announcable *session)
{
	if(session == this)
		sendUpdatedAnnouncementList();
}

void Session::onAnnouncementError(const Announcable *session, const QString &message)
{
	if(session == this) {
		messageAll(message, false);
	}
}

void Session::onConfigValueChanged(const ConfigKey &key)
{
	if(key.index == config::ForceNsfm.index) {
		if(forceEnableNsfm(m_history, m_config)) {
			log(Log().about(Log::Level::Info, Log::Topic::Status).message("Forced NSFM after config change"));
		}
		sendUpdatedSessionProperties();
	}
}

void Session::sendAbuseReport(const Client *reporter, int aboutUser, const QString &message)
{
	Q_ASSERT(reporter);

	reporter->log(Log().about(Log::Level::Info, Log::Topic::Status).message(QString("Abuse report about user %1 received: %2").arg(aboutUser).arg(message)));

	const QUrl url = m_config->internalConfig().reportUrl;
	if(!url.isValid()) {
		// This shouldn't happen normally. If the URL is not configured,
		// the server does not advertise the capability to receive reports.
		log(Log().about(Log::Level::Warn, Log::Topic::Status).message("Cannot send abuse report: server URL not configured!"));
		return;
	}

	if(!m_config->getConfigBool(config::AbuseReport)) {
		// This can happen if reporting is disabled when a session is still in progress
		log(Log().about(Log::Level::Warn, Log::Topic::Status).message("Cannot send abuse report: not enabled!"));
		return;
	}

	QJsonObject o;
	o["session"] = id();
	o["sessionTitle"] = m_history->title();
	o["user"] = reporter->username();
	o["auth"] = reporter->isAuthenticated();
	o["ip"] = reporter->peerAddress().toString();
	if(aboutUser>0)
		o["perp"] = aboutUser;

	o["message"] = message;
	o["offset"] = int(m_history->sizeInBytes());
	QJsonArray users;
	for(const Client *c : m_clients) {
		QJsonObject u;
		u["name"] = c->username();
		u["auth"] = c->isAuthenticated();
		u["op"] = c->isOperator();
		u["ip"] = c->peerAddress().toString();
		u["id"] = c->id();
		users.append(u);
	}
	o["users"] = users;

	const QString authToken = m_config->getConfigString(config::ReportToken);

	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	if(!authToken.isEmpty())
		req.setRawHeader("Authorization", "Token " + authToken.toUtf8());
	QNetworkReply *reply = networkaccess::getInstance()->post(req, QJsonDocument(o).toJson());
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		if(reply->error() != QNetworkReply::NoError) {
			log(Log().about(Log::Level::Warn, Log::Topic::Status).message("Unable to send abuse report: " + reply->errorString()));
		}
	});
	connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

QJsonObject Session::getDescription(bool full) const
{
	// The basic description contains just the information
	// needed for the login session listing
	QJsonObject o {
		{"id", id()},
		{"alias", idAlias()},
		{"protocol", m_history->protocolVersion().asString()},
		{"userCount", userCount()},
		{"maxUserCount", m_history->maxUsers()},
		{"founder", m_history->founderName()},
		{"title", m_history->title()},
		{"hasPassword", !m_history->passwordHash().isEmpty()},
		{"closed", isClosed()},
		{"authOnly", m_history->hasFlag(SessionHistory::AuthOnly)},
		{"nsfm", m_history->hasFlag(SessionHistory::Nsfm)},
		{"startTime", m_history->startTime().toString(Qt::ISODate)},
		{"size", int(m_history->sizeInBytes())}
	};

	if(m_config->getConfigBool(config::EnablePersistence))
		o["persistent"] = m_history->hasFlag(SessionHistory::Persistent);

	if(full) {
		// Full descriptions includes detailed info for server admins.
		o["maxSize"] = int(m_history->sizeLimit());
		o["resetThreshold"] = int(m_history->autoResetThreshold());
		o["deputies"] = m_history->hasFlag(SessionHistory::Deputies);
		o["hasOpword"] = !m_history->opwordHash().isEmpty();

		QJsonArray users;
		for(const Client *user : m_clients) {
			QJsonObject u = user->description(false);
			u["online"] = true;
			users << u;
		}
		for(auto u=m_pastClients.constBegin();u!=m_pastClients.constEnd();++u) {
			users << QJsonObject {
				{"id", u->id},
				{"name", u->username},
				{"ip", u->peerAddress.toString()},
				{"online", false}
			};
		}
		o["users"] = users;

		QJsonArray listings;
		const auto announcements = m_announcements->getAnnouncements(this);
		for(const sessionlisting::Announcement &a : announcements) {
			listings << QJsonObject {
				{"id", a.listingId},
				{"url", a.apiUrl.toString()},
				{"roomcode", a.roomcode},
				{"private", a.isPrivate}
			};
		}
		o["listings"] = listings;
	}

	return o;
}

JsonApiResult Session::callJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	if(!path.isEmpty()) {
		QString head;
		QStringList tail;
		std::tie(head, tail) = popApiPath(path);

		if(head == "listing")
			return callListingsJsonApi(method, tail, request);

		int userId = head.toInt();
		if(userId>0) {
			Client *c = getClientById(userId);
			if(c)
				return c->callJsonApi(method, tail, request);
		}

		return JsonApiNotFound();
	}

	if(method == JsonApiMethod::Update) {
		setSessionConfig(request, nullptr);

		if(request.contains("message"))
			messageAll(request["message"].toString(), false);
		if(request.contains("alert"))
			messageAll(request["alert"].toString(), true);

	} else if(method == JsonApiMethod::Delete) {
		killSession();
		return JsonApiResult{ JsonApiResult::Ok, QJsonDocument(QJsonObject{ { "status", "ok "} }) };
	}

	return JsonApiResult{JsonApiResult::Ok, QJsonDocument(getDescription(true))};
}

JsonApiResult Session::callListingsJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	Q_UNUSED(request);
	if(path.length() != 1)
		return JsonApiNotFound();
	const int id = path.at(0).toInt();

	const auto announcements = m_announcements->getAnnouncements(this);
	for(const sessionlisting::Announcement &a : announcements) {
		if(a.listingId == id) {
			if(method == JsonApiMethod::Delete) {
				unlistAnnouncement(a.apiUrl.toString());
				return JsonApiResult{ JsonApiResult::Ok, QJsonDocument(QJsonObject{ { "status", "ok "} }) };

			} else {
				return JsonApiBadMethod();
			}
		}
	}
	return JsonApiNotFound();
}

void Session::log(const Log &log)
{
	Log entry = log;
	entry.session(id());
	m_config->logger()->logMessage(entry);

	if(entry.level() < Log::Level::Debug) {
		directToAll(makeLogMessage(entry));
	}
}

}

