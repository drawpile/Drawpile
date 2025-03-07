// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/output.h>
#include <dpengine/recorder.h>
}
#include "libserver/announcements.h"
#include "libserver/client.h"
#include "libserver/opcommands.h"
#include "libserver/serverconfig.h"
#include "libserver/serverlog.h"
#include "libserver/session.h"
#include "libshared/net/servercmd.h"
#include "libshared/util/filename.h"
#include "libshared/util/networkaccess.h"
#include "libshared/util/passwordhash.h"
#include "libshared/util/qtcompat.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace server {

class Session::AdminChat {
public:
	AdminChat(Session *session, int timeoutMs)
	{
		m_timer.setTimerType(Qt::CoarseTimer);
		m_timer.setSingleShot(true);
		m_timer.setInterval(timeoutMs);
		connect(
			&m_timer, &QTimer::timeout, session, &Session::timeOutAdminChat);
		m_timer.start();
	}

	int timeoutMs() const { return m_timer.interval(); }

	void restartTimer() { m_timer.start(); }

	void addChatMessage(
		Session *session, int id, const QHostAddress &ip, const QString &name,
		const QString &message, int flags)
	{
		limitMessages();
		QJsonObject o{
			{QStringLiteral("i"), id},
			{QStringLiteral("n"), name},
			{QStringLiteral("m"), message},
		};
		if(flags != 0) {
			o[QStringLiteral("f")] = flags;
		}
		m_messages.append(o);
		session->m_config->logger()->logMessage(
			Log()
				.about(Log::Level::Info, Log::Topic::AdminChat)
				.session(session->id())
				.user(id, ip, name)
				.message(message));
	}

	void addAdminMessage(Session *session, const QString &message)
	{
		limitMessages();
		m_messages.append(QJsonObject{
			{QStringLiteral("m"), message},
		});
		session->m_config->logger()->logMessage(
			Log()
				.about(Log::Level::Info, Log::Topic::AdminChat)
				.session(session->id())
				.message(message));
	}

	QJsonArray getMessages(int *inOutOffset) const
	{
		QJsonArray messages;
		int count = m_messages.size();
		int offset = qMax(0, *inOutOffset - m_offset);
		for(int i = offset; i < count; ++i) {
			messages.append(m_messages[i]);
		}
		*inOutOffset = offset + m_offset;
		return messages;
	}

	QJsonObject getDescription() const
	{
		return QJsonObject{
			{QStringLiteral("offset"), m_offset},
			{QStringLiteral("count"), int(m_messages.size())},
			{QStringLiteral("timeout"), m_timer.interval()},
			{QStringLiteral("remaining"), m_timer.remainingTime()},
		};
	}

private:
	void limitMessages()
	{
		if(m_messages.size() >= 1024) {
			m_messages.removeFirst();
			++m_offset;
		}
	}

	QVector<QJsonObject> m_messages;
	QTimer m_timer;
	int m_offset = 0;
};

static bool forceEnableNsfm(SessionHistory *history, const ServerConfig *config)
{
	if(config->getConfigBool(config::ForceNsfm)) {
		history->setFlag(SessionHistory::Nsfm);
		return true;
	} else {
		return false;
	}
}

Session::Session(
	SessionHistory *history, ServerConfig *config,
	sessionlisting::Announcements *announcements, QObject *parent)
	: QObject(parent)
	, m_history(history)
	, m_config(config)
	, m_announcements(announcements)
{
	m_history->setParent(this);
	connect(
		m_config, &ServerConfig::configValueChanged, this,
		&Session::onConfigValueChanged);
	forceEnableNsfm(m_history, m_config);

	m_lastEventTime.start();

	// History already exists? Skip the Initialization state.
	if(history->sizeInBytes() > 0)
		m_state = State::Running;

	// Session announcements
	connect(
		m_announcements, &sessionlisting::Announcements::announcementsChanged,
		this, &Session::onAnnouncementsChanged);
	connect(
		m_announcements, &sessionlisting::Announcements::announcementError,
		this, &Session::onAnnouncementError);
	for(const QString &announcement : m_history->announcements())
		makeAnnouncement(QUrl(announcement));
}

Session::~Session()
{
	delete m_adminChat;
	m_adminChat = nullptr;
	emit sessionDestroyed(this);
}

static net::Message makeLogMessage(const Log &log)
{
	return net::ServerReply::makeLog(
		log.message(), log.toJson(Log::NoPrivateData | Log::NoSession));
}

net::MessageList Session::serverSideStateMessages() const
{
	net::MessageList msgs;
	QVector<uint8_t> owners;
	QVector<uint8_t> trusted;

	for(const Client *c : m_clients) {
		if(!c->isGhost()) {
			msgs.append(c->joinMessage());
		}
		if(c->isOperator()) {
			owners.append(c->id());
		}
		if(c->isTrusted()) {
			trusted.append(c->id());
		}
	}

	msgs.append(net::makeSessionOwnerMessage(0, owners));
	if(!trusted.isEmpty()) {
		msgs.append(net::makeTrustedUsersMessage(0, trusted));
	}

	return msgs;
}

void Session::switchState(State newstate)
{
	if(newstate == State::Initialization) {
		qFatal("Illegal state change to Initialization from %d", int(m_state));

	} else if(newstate == State::Running) {
		if(m_state != State::Initialization && m_state != State::Reset)
			qFatal("Illegal state change to Running from %d", int(m_state));

		m_initUser = -1;
		bool success = true;

		if(m_state == State::Initialization) {
			m_history->resetAutoResetThresholdBase();
			log(Log()
					.about(Log::Level::Info, Log::Topic::Status)
					.message(QStringLiteral("Session initialized with size %1")
								 .arg(QLocale::c().formattedDataSize(
									 m_history->sizeInBytes()))));
			onSessionInitialized();

		} else if(m_state == State::Reset && !m_resetstream.isEmpty()) {
			// Reset buffer uploaded. Now perform the reset before returning to
			// normal running state.

			auto resetImage = serverSideStateMessages() + m_resetstream;

			// Send reset snapshot
			if(!m_history->reset(resetImage)) {
				// This shouldn't normally happen, as the size limit should be
				// caught while still uploading the reset.
				directToAll(net::ServerReply::makeKeyAlertReset(
					QStringLiteral("Session reset failed!"),
					QStringLiteral("failed"),
					net::ServerReply::KEY_RESET_FAILED));
				success = false;

			} else {
				directToAll(net::ServerReply::makeReset(
					QStringLiteral("Session reset!"), QStringLiteral("reset")));

				onSessionReset();

				sendUpdatedSessionProperties();
			}

			m_resetstream = net::MessageList{};
			m_resetstreamsize = 0;
		}

		if(success && !m_recordingFile.isEmpty())
			restartRecording();

		for(Client *c : m_clients)
			c->setHoldLocked(false);

	} else if(newstate == State::Reset) {
		if(m_state != State::Running) {
			qFatal("Illegal state change to Reset from %d", int(m_state));
		}

		m_resetstream.clear();
		m_resetstreamsize = 0;
	}

	m_state = newstate;
	onStateChanged();
}

void Session::chatMessageToAll(const net::Message &msg)
{
	directToAll(msg);
}

void Session::assignId(Client *user)
{
	uint8_t id = m_history->idQueue().getIdForName(user->username());

	int loops = 256;
	while(loops > 0 && (id == 0 || getClientById(id))) {
		id = m_history->idQueue().nextId();
		--loops;
	}
	Q_ASSERT(loops > 0); // shouldn't happen, since we don't let new users in if
						 // the session is full
	user->setId(id);
}

void Session::joinUser(Client *user, bool host, const Invite *invite)
{
	Client *existingUser = getClientById(user->id());
	user->setSession(this);
	m_clients.append(user);

	connect(user, &Client::loggedOff, this, &Session::removeUser);

	const QString &username = user->username();
	if(existingUser) {
		existingUser->disconnectClient(
			Client::DisconnectionReason::Kick, username,
			QStringLiteral("Replaced by rejoining"));
	}

	m_pastClients.remove(user->id());

	if(host) {
		Q_ASSERT(m_state == State::Initialization);
		m_initUser = user->id();

	} else {
		if(m_state == State::Initialization)
			user->setHoldLocked(true);
	}

	onClientJoin(user, host);

	const QString &authId = user->authId();
	bool isGhost = user->isGhost();
	if(isGhost) {
		user->sendSystemChat(
			QStringLiteral(
				"GHOST MODE. Your presence is not overtly announced, chat "
				"messages are treated as server alerts. You are not hidden "
				"though, users can see your presence in the event log and "
				"taking administrative actions will show your name in chat."),
			true);
	} else {
		addToHistory(user->joinMessage());
		if(!authId.isEmpty()) {
			m_history->setAuthenticatedUsername(authId, username);
		}
	}

	if(user->isOperator() || m_history->isOperator(authId)) {
		changeOpStatus(user->id(), true, QString(), false);
	} else if(invite && invite->op) {
		changeOpStatus(user->id(), true, invite->creator, false, invite);
	}

	if(user->authFlags().contains("TRUSTED") || m_history->isTrusted(authId)) {
		changeTrustedStatus(user->id(), true, QString(), false);
	} else if(invite && invite->trust) {
		changeTrustedStatus(user->id(), true, invite->creator, false, invite);
	}

	ensureOperatorExists();

	// Send session log history to the new client
	bool mod = user->isModerator();
	QList<Log> log = m_config->logger()
						 ->query()
						 .session(id())
						 .atleast(Log::Level::Info)
						 .omitSensitive(!mod)
						 .omitKicksAndBans(!mod && !user->isOperator())
						 .page(0, 1000)
						 .get();
	// Note: the query returns the log entries in latest first, but we send
	// new entries to clients as they occur, so we reverse the list before
	// sending it
	for(int i = log.size() - 1; i >= 0; --i) {
		user->sendDirectMessage(makeLogMessage(log.at(i)));
	}

	const QString welcomeMessage =
		m_config->getConfigString(config::WelcomeMessage);
	if(!welcomeMessage.isEmpty() && !user->sendSystemChat(welcomeMessage)) {
		user->log(Log()
					  .about(Log::Level::Error, Log::Topic::Status)
					  .message(QStringLiteral("Welcome message too long")));
	}

	// Make sure everyone is up to date
	sendUpdatedAnnouncementList();
	sendUpdatedBanlist();
	sendUpdatedMuteList();
	sendUpdatedAuthList();
	sendUpdatedInviteList();

	m_history->joinUser(user->id(), username);

	user->log(Log()
				  .about(Log::Level::Info, Log::Topic::Join)
				  .message(
					  isGhost ? QStringLiteral(
									"Moderator in ghost mode joined session")
							  : QStringLiteral("Joined session")));
	emit sessionAttributeChanged(this);
}

void Session::removeUser(Client *user)
{
	if(!m_clients.removeOne(user))
		return;

	m_pastClients.insert(
		user->id(), {user->id(), user->authId(), user->username(),
					 user->peerAddress(), user->sid(), !user->isModerator()});

	Q_ASSERT(user->session() == this);
	bool isGhost = user->isGhost();
	user->log(
		Log()
			.about(Log::Level::Info, Log::Topic::Leave)
			.message(
				isGhost ? QStringLiteral("Moderator in ghost mode left session")
						: QStringLiteral("Left session")));
	user->setSession(nullptr);

	disconnect(user, nullptr, this, nullptr);
	disconnect(m_history, nullptr, user, nullptr);

	onClientLeave(user);

	if(!isGhost) {
		addToHistory(net::makeLeaveMessage(user->id()));
	}
	// Try not to reuse the ID right away
	m_history->idQueue().reserveId(user->id());

	ensureOperatorExists();

	// Reopen the session when the last user leaves
	if(m_clients.isEmpty()) {
		setClosed(false);
	}

	emit sessionAttributeChanged(this);
}

void Session::onClientLeave(Client *client)
{
	if(client->id() == m_initUser && m_state == State::Reset) {
		// Whoops, the resetter left before the job was done!
		// We simply cancel the reset in that case and go on
		abortReset();
	}
}

void Session::abortReset()
{
	m_initUser = -1;
	m_resetstream.clear();
	m_resetstreamsize = 0;
	switchState(State::Running);
	directToAll(net::ServerReply::makeKeyAlertReset(
		QStringLiteral("Session reset cancelled."), QStringLiteral("cancel"),
		net::ServerReply::KEY_RESET_CANCEL));
}

Client *Session::getClientById(uint8_t id)
{
	for(Client *c : m_clients) {
		if(c->id() == id) {
			return c;
		}
	}
	return nullptr;
}

Client *Session::getClientByAuthId(const QString &authId)
{
	if(!authId.isEmpty()) {
		for(Client *c : m_clients) {
			if(c->authId() == authId) {
				return c;
			}
		}
	}
	return nullptr;
}

Client *Session::getClientByUsername(const QString &username)
{
	for(Client *c : m_clients) {
		if(c->username().compare(username, Qt::CaseInsensitive) == 0) {
			return c;
		}
	}
	return nullptr;
}

bool Session::addBan(
	const Client *target, const QString &bannedBy, const Client *client)
{
	Q_ASSERT(target);
	if(m_history->addBan(
		   target->username(), target->peerAddress(), target->authId(),
		   target->sid(), bannedBy, client)) {
		target->log(Log()
						.about(Log::Level::Info, Log::Topic::Ban)
						.message("Banned by " + bannedBy));
		sendUpdatedBanlist();
		return true;
	} else {
		return false;
	}
}

bool Session::addPastBan(
	const PastClient &target, const QString &bannedBy, const Client *client)
{
	Q_ASSERT(target.id > 0);
	if(m_history->addBan(
		   target.username, target.peerAddress, target.authId, target.sid,
		   bannedBy, client)) {
		log(Log()
				.user(target.id, target.peerAddress, target.username)
				.about(Log::Level::Info, Log::Topic::Ban)
				.message("Banned by " + bannedBy));
		sendUpdatedBanlist();
		return true;
	} else {
		return false;
	}
}

bool Session::isBanned(const Client *target) const
{
	return m_history->banlist().isBanned(
		target->username(), target->peerAddress(), target->authId(),
		target->sid());
}

bool Session::isPastBanned(const PastClient &target) const
{
	return m_history->banlist().isBanned(
		target.username, target.peerAddress, target.authId, target.sid);
}

void Session::removeBan(int entryId, const QString &removedBy)
{
	QString unbanned = m_history->removeBan(entryId);
	if(!unbanned.isEmpty()) {
		log(Log()
				.about(Log::Level::Info, Log::Topic::Unban)
				.message(unbanned + " unbanned by " + removedBy));
		sendUpdatedBanlist();
	}
}

int Session::activeDrawingUserCount(qint64 ms) const
{
	qint64 now = QDateTime::currentMSecsSinceEpoch();
	int count = 0;
	for(const Client *c : m_clients) {
		if(now - c->lastActiveDrawing() <= ms) {
			++count;
		}
	}
	return count;
}

bool Session::isEffectivelyEmpty() const
{
	for(const Client *c : m_clients) {
		if(!c->isGhost()) {
			return false;
		}
	}
	return true;
}

void Session::setRecordingFile(const QString &filename)
{
	m_recordingFile = filename;
	if(m_state == State::Running && !m_recorder && !m_recordingFile.isEmpty()) {
		restartRecording();
	}
}

bool Session::isClosed(bool ignoreFlag) const
{
	return (!ignoreFlag && m_closed) || userCount() >= m_history->maxUsers() ||
		   (m_state != State::Initialization && m_state != State::Running);
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
		// Otherwise it would be possible for users to accidentally lock
		// themselves out.
		if(!authOnly || !changedBy || changedBy->isAuthenticated()) {
			flags.setFlag(SessionHistory::AuthOnly, authOnly);
			changes
				<< (authOnly ? "blocked guest logins"
							 : "permitted guest logins");
		}
	}

	bool changedByModeratorOrAdmin = !changedBy || changedBy->isModerator();
	if(conf.contains(QStringLiteral("persistent")) &&
	   (changedByModeratorOrAdmin ||
		(changedBy &&
		 changedBy->authFlags().contains(QStringLiteral("PERSIST"))) ||
		m_config->getConfigBool(config::EnablePersistence))) {
		bool persistent = conf[QStringLiteral("persistent")].toBool();
		flags.setFlag(SessionHistory::Persistent, persistent);
		changes.append(
			persistent ? QStringLiteral("made persistent")
					   : QStringLiteral("made nonpersistent"));
	}

	bool changeFounder =
		changedByModeratorOrAdmin && conf.contains(QStringLiteral("founder"));
	if(changeFounder) {
		m_history->setFounderName(
			conf.value(QStringLiteral("founder")).toString());
		changes << QStringLiteral("changed founder");
	}

	if((changeFounder && flags.testFlag(SessionHistory::AutoTitle)) ||
	   conf.value(QString("autotitle")).toBool()) {
		bool hadAutoTitle = flags.testFlag(SessionHistory::AutoTitle);
		flags.setFlag(SessionHistory::AutoTitle, true);
		QString newTitle = QStringLiteral("%1 Drawpile")
							   .arg(m_history->founderName())
							   .mid(0, 100);
		if(m_history->setTitle(newTitle)) {
			changes.append(
				QStringLiteral("changed autotitle to '%1'").arg(newTitle));
		} else if(!hadAutoTitle) {
			changes.append(QStringLiteral("enabled autotitle"));
		}
	} else if(conf.contains(QStringLiteral("title"))) {
		bool hadAutoTitle = flags.testFlag(SessionHistory::AutoTitle);
		flags.setFlag(SessionHistory::AutoTitle, false);
		QString newTitle =
			conf.value(QStringLiteral("title")).toString().mid(0, 100);
		if(m_history->setTitle(newTitle)) {
			changes.append(
				QStringLiteral("changed title to '%1'").arg(newTitle));
		} else if(hadAutoTitle) {
			changes.append(QStringLiteral("disabled autotitle"));
		}
	}

	if(conf.contains("maxUserCount")) {
		int maxUsers = qBound(2, conf["maxUserCount"].toInt(), 254);
		int prevMaxUsers = m_history->maxUsers();
		int userLimit =
			qBound(2, m_config->getConfigInt(config::SessionUserLimit), 254);
		bool maxUsersInBounds = changedByModeratorOrAdmin ||
								maxUsers <= prevMaxUsers ||
								maxUsers <= userLimit;
		int effectiveMaxUsers =
			maxUsersInBounds ? maxUsers
							 : qMin(maxUsers, qMax(prevMaxUsers, userLimit));
		m_history->setMaxUsers(effectiveMaxUsers);
		changes << "changed max. user count";
	}

	if(conf.contains("resetThreshold")) {
		int val;
		if(conf["resetThreshold"].isDouble())
			val = conf["resetThreshold"].toInt();
		else
			val = ServerConfig::parseSizeString(
				conf["resetThreshold"].toString());
		m_history->setAutoResetThreshold(val);
		changes << "changed autoreset threshold";
	}

	bool changePassword = conf.contains(QStringLiteral("password"));
	QString newPassword =
		changePassword ? conf.value(QStringLiteral("password")).toString()
					   : QString();

	if(conf.contains("opword")) {
		m_history->setOpword(conf["opword"].toString());
		changes << "changed opword";
	}

	// Note: this bit is only relayed by the server: it informs
	// the client whether to send preserved/recorded chat messages
	// by default.
	if(conf.contains("preserveChat")) {
		flags.setFlag(
			SessionHistory::PreserveChat, conf["preserveChat"].toBool());
		changes
			<< (conf["preserveChat"].toBool() ? "preserve chat"
											  : "don't preserve chat");
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
		changes
			<< (conf["deputies"].toBool() ? "enabled deputies"
										  : "disabled deputies");
	}

	// Changing the idle override is only allowed if it's configured and the
	// client is a moderator or the request came from the API (null changedBy.)
	bool changeIdleOverride =
		conf.contains(QStringLiteral("idleOverride")) &&
		m_config->getConfigBool(config::AllowIdleOverride) &&
		changedByModeratorOrAdmin;
	if(changeIdleOverride) {
		bool idleOverride = conf[QStringLiteral("idleOverride")].toBool();
		flags.setFlag(SessionHistory::IdleOverride, idleOverride);
		changes
			<< (idleOverride ? QStringLiteral("enabled idle override")
							 : QStringLiteral("disabled idle override"));
	}

	if(changedByModeratorOrAdmin && conf.contains(QStringLiteral("invites"))) {
		bool invites = conf.value(QStringLiteral("invites")).toBool();
		flags.setFlag(SessionHistory::Invites, invites);
		changes.append(
			invites ? QStringLiteral("enabled invites")
					: QStringLiteral("disabled invites"));
	}

	// Toggling allowing WebSocket connections requires the client to have the
	// WEBSESSION flag. If changedBy is null, the request came from the API.
	bool canManageWebSession = !changedBy || changedBy->canManageWebSession();
	bool changeAllowWeb =
		conf.contains(QStringLiteral("allowWeb")) && canManageWebSession;
	if(changeAllowWeb) {
		bool allowWeb = conf[QStringLiteral("allowWeb")].toBool();
		// We don't allow clients that are connected via browser to prevent
		// themselves from rejoining the session by disabling them.
		if(allowWeb || !changedBy || !changedBy->isBrowser()) {
			flags.setFlag(SessionHistory::AllowWeb, allowWeb);
			changes
				<< (allowWeb ? QStringLiteral("enabled browser connections")
							 : QStringLiteral("disabled browser connections"));
		}
	}

	if(changePassword && !changeAllowWeb &&
	   m_config->getConfigBool(config::PasswordDependentWebSession)) {
		bool hasPassword = !newPassword.isEmpty();
		bool allowWeb = flags.testFlag(SessionHistory::AllowWeb);
		if(hasPassword && !allowWeb) {
			flags.setFlag(SessionHistory::AllowWeb, true);
			changes << QStringLiteral(
				"enabled browser connections because password is set");
		} else if(!hasPassword && allowWeb && !canManageWebSession) {
			if(changedBy && changedBy->isBrowser()) {
				// Don't let browser users obliterate themselves.
				changePassword = false;
			} else {
				flags.setFlag(SessionHistory::AllowWeb, false);
				kickWebUsers(changedBy);
				changes << QStringLiteral(
					"disabled browser connections because no password is set");
			}
		}
	}

	if(changePassword) {
		m_history->setPassword(newPassword);
		changes << QStringLiteral("changed password");
	}

	m_history->setFlags(flags);

	if(!changes.isEmpty()) {
		sendUpdatedSessionProperties();
		if(changePassword) {
			net::Message msg =
				net::ServerReply::makePasswordChange(newPassword);
			for(Client *c : m_clients) {
				if(!c->hasInviteSecret()) {
					c->sendDirectMessage(msg);
				}
			}
		}

		QString logmsg = changes.join(QStringLiteral(", "));
		logmsg[0] = logmsg[0].toUpper();
		Log l =
			Log().about(Log::Level::Info, Log::Topic::Status).message(logmsg);
		if(changedBy) {
			changedBy->log(l);
		} else {
			log(l);
		}
	}
}

QVector<uint8_t> Session::updateOwnership(
	QVector<uint8_t> ids, const QString &changedBy, bool sendUpdate,
	const Invite *invite)
{
	QVector<uint8_t> truelist;
	Client *kickResetter = nullptr;
	bool needsUpdate = false;
	for(Client *c : m_clients) {
		const bool op = ids.contains(c->id()) || c->isModerator();
		if(op != c->isOperator()) {
			needsUpdate = true;
			if(!op) {
				onClientDeop(c);
				if(c->id() == m_initUser && m_state == State::Reset) {
					// OP status removed mid-reset! The user probably has at
					// least part of the reset image still queued for upload,
					// which will messs up the session once we're out of reset
					// mode. Kicking the client is the easiest workaround.
					// TODO for 3.0: send a cancel command to the client and
					// ignore all further input until ack is received.
					kickResetter = c;
				}
			}

			c->setOperator(op);
			bool isGhost = c->isGhost();
			Log::Topic topic;
			QString msg, key;
			if(op) {
				topic = Log::Topic::Op;
				if(invite) {
					key = net::ServerReply::KEY_OP_GIVE_INVITE;
					msg =
						QStringLiteral("Made operator via invite created by %2")
							.arg(
								invite->creator.isEmpty()
									? QStringLiteral("a server administrator")
									: invite->creator);
				} else {
					key = net::ServerReply::KEY_OP_GIVE;
					msg = QStringLiteral("Made operator by %1")
							  .arg(
								  changedBy.isEmpty()
									  ? QStringLiteral("the server")
									  : changedBy);
				}
			} else {
				topic = Log::Topic::Deop;
				key = net::ServerReply::KEY_OP_TAKE;
				msg = QStringLiteral("Operator status revoked by %1")
						  .arg(
							  changedBy.isEmpty() ? QStringLiteral("the server")
												  : changedBy);
			}

			c->log(Log().about(Log::Level::Info, topic).message(msg));

			if(!isGhost) {
				QJsonObject params;
				params.insert(QStringLiteral("target"), c->username());
				if(invite) {
					if(!invite->creator.isEmpty()) {
						params.insert(
							QStringLiteral("creator"), invite->creator);
					}
				} else if(!changedBy.isEmpty()) {
					params.insert(QStringLiteral("by"), changedBy);
				}
				msg[0] = msg[0].toLower();
				keyMessageAll(
					QStringLiteral("%1 %2").arg(c->username(), msg), false, key,
					params);

				if(c->isAuthenticated() && !c->isModerator()) {
					m_history->setAuthenticatedOperator(c->authId(), op);
				}
			}
		}
		if(c->isOperator()) {
			truelist.append(c->id());
		}
	}

	if(kickResetter) {
		kickResetter->disconnectClient(
			Client::DisconnectionReason::Error, "De-opped while resetting",
			changedBy.isEmpty() ? QStringLiteral("by the server")
								: QStringLiteral("by %1").arg(changedBy));
	}

	if(sendUpdate && needsUpdate) {
		sendUpdatedAuthList();
		sendUpdatedBanlist(true);
		sendUpdatedInviteList();
	}

	return truelist;
}

Invite *Session::createInvite(Client *client, int maxUses, bool trust, bool op)
{
	Invite *invite = m_history->createInvite(
		client ? client->username() : QString(), maxUses, trust, op);
	if(invite) {
		logFor(
			client,
			Log()
				.about(Log::Level::Info, Log::Topic::Invite)
				.message(QStringLiteral("Created %1invite with %2 use(s)")
							 .arg(
								 op && trust ? QStringLiteral("op and trust ")
								 : op		 ? QStringLiteral("op ")
								 : trust	 ? QStringLiteral("trust ")
											 : QString(),
								 invite->maxUses > 0
									 ? QString::number(invite->maxUses)
									 : QStringLiteral("unlimited"))));
		sendUpdatedInviteList();
		return invite;
	} else {
		return nullptr;
	}
}

bool Session::removeInvite(Client *client, const QString &secret)
{
	bool removed = m_history->removeInvite(secret);
	if(removed) {
		logFor(
			client,
			Log()
				.about(Log::Level::Info, Log::Topic::Invite)
				.message(QStringLiteral("Removed invite %1").arg(secret)));
		sendUpdatedInviteList();
		return true;
	} else {
		return false;
	}
}

bool Session::hasInvite(const QString &secret) const
{
	return m_history->invitesBySecret().contains(secret);
}

CheckInviteResult Session::checkInvite(
	Client *client, const QString &secret, bool use, Invite **outInvite)
{
	CheckInviteResult result =
		m_history->checkInvite(client, secret, use, nullptr, outInvite);
	switch(result) {
	case CheckInviteResult::InviteUsed:
		log(client->setUserOnLog(
			Log()
				.about(Log::Level::Info, Log::Topic::Invite)
				.message(QStringLiteral("Used invite %1").arg(secret))));
		Q_FALLTHROUGH();
	case CheckInviteResult::AlreadyInvitedNameChanged:
		sendUpdatedInviteList();
		break;
	default:
		break;
	}
	return result;
}

void Session::changeOpStatus(
	uint8_t id, bool op, const QString &changedBy, bool sendUpdate,
	const Invite *invite)
{
	QVector<uint8_t> ids;
	for(const Client *c : m_clients) {
		if(c->isOperator()) {
			ids.append(c->id());
		}
	}

	if(op) {
		ids.append(id);
	} else {
		ids.removeOne(id);
	}

	ids = updateOwnership(ids, changedBy, sendUpdate, invite);
	addToHistory(net::makeSessionOwnerMessage(0, ids));
}

QVector<uint8_t> Session::updateTrustedUsers(
	QVector<uint8_t> ids, const QString &changedBy, bool sendUpdate,
	const Invite *invite)
{
	QVector<uint8_t> truelist;
	bool needsUpdate = false;
	for(Client *c : m_clients) {
		bool trusted = ids.contains(c->id());
		if(trusted != c->isTrusted()) {
			needsUpdate = true;
			c->setTrusted(trusted);
			bool isGhost = c->isGhost();
			Log::Topic topic;
			QString msg, key;
			if(trusted) {
				topic = Log::Topic::Trust;
				if(invite) {
					key = net::ServerReply::KEY_TRUST_GIVE_INVITE;
					msg = QStringLiteral("Trusted via invite created by %2")
							  .arg(
								  invite->creator.isEmpty()
									  ? QStringLiteral("a server administrator")
									  : invite->creator);
				} else {
					key = net::ServerReply::KEY_TRUST_GIVE;
					msg = QStringLiteral("Trusted by %1")
							  .arg(
								  changedBy.isEmpty()
									  ? QStringLiteral("the server")
									  : changedBy);
				}
			} else {
				topic = Log::Topic::Untrust;
				key = net::ServerReply::KEY_TRUST_TAKE;
				msg = QStringLiteral("Untrusted by %1")
						  .arg(
							  changedBy.isEmpty() ? QStringLiteral("the server")
												  : changedBy);
			}

			c->log(Log().about(Log::Level::Info, topic).message(msg));

			if(!isGhost) {
				QJsonObject params;
				params.insert(QStringLiteral("target"), c->username());
				if(invite) {
					if(!invite->creator.isEmpty()) {
						params.insert(
							QStringLiteral("creator"), invite->creator);
					}
				} else if(!changedBy.isEmpty()) {
					params.insert(QStringLiteral("by"), changedBy);
				}
				msg[0] = msg[0].toLower();
				keyMessageAll(
					QStringLiteral("%1 %2").arg(c->username(), msg), false, key,
					params);

				if(c->isAuthenticated()) {
					m_history->setAuthenticatedTrust(c->authId(), trusted);
				}
			}
		}
		if(c->isTrusted()) {
			truelist.append(c->id());
		}
	}

	if(sendUpdate && needsUpdate) {
		sendUpdatedAuthList();
		sendUpdatedInviteList();
	}

	return truelist;
}

void Session::changeTrustedStatus(
	uint8_t id, bool trusted, const QString &changedBy, bool sendUpdate,
	const Invite *invite)
{
	QVector<uint8_t> ids;
	for(const Client *c : m_clients) {
		if(c->isTrusted()) {
			ids.append(c->id());
		}
	}

	if(trusted) {
		ids.append(id);
	} else {
		ids.removeOne(id);
	}

	ids = updateTrustedUsers(ids, changedBy, sendUpdate, invite);
	addToHistory(net::makeTrustedUsersMessage(0, ids));
}

void Session::sendUpdatedSessionProperties()
{
	QJsonObject config = {
		// this refers specifically to the closed flag, not the general status
		{QStringLiteral("closed"), m_closed},
		{QStringLiteral("authOnly"),
		 m_history->hasFlag(SessionHistory::AuthOnly)},
		{QStringLiteral("persistent"),
		 m_history->hasFlag(SessionHistory::Persistent)},
		{QStringLiteral("title"), m_history->title()},
		{QStringLiteral("autotitle"),
		 m_history->hasFlag(SessionHistory::AutoTitle)},
		{QStringLiteral("maxUserCount"), m_history->maxUsers()},
		{QStringLiteral("resetThreshold"),
		 int(m_history->autoResetThreshold())},
		{QStringLiteral("resetThresholdBase"),
		 int(m_history->autoResetThresholdBase())},
		{QStringLiteral("preserveChat"),
		 m_history->hasFlag(SessionHistory::PreserveChat)},
		{QStringLiteral("nsfm"), m_history->hasFlag(SessionHistory::Nsfm)},
		{QStringLiteral("deputies"),
		 m_history->hasFlag(SessionHistory::Deputies)},
		{QStringLiteral("hasPassword"), !m_history->passwordHash().isEmpty()},
		{QStringLiteral("hasOpword"), !m_history->opwordHash().isEmpty()},
		{QStringLiteral("idleOverride"),
		 m_history->hasFlag(SessionHistory::IdleOverride)},
		{QStringLiteral("invites"),
		 m_history->hasFlag(SessionHistory::Invites)},
		// These configs are basically session properties set by the server.
		// We report them here so the client can show them in the UI.
		{QStringLiteral("forceNsfm"),
		 m_config->getConfigBool(config::ForceNsfm)},
		{QStringLiteral("idleTimeLimit"),
		 m_config->getConfigTime(config::IdleTimeLimit)},
		{QStringLiteral("allowIdleOverride"),
		 m_config->getConfigBool(config::AllowIdleOverride)},
	};
#ifdef HAVE_WEBSOCKETS
	if(m_config->internalConfig().webSocket) {
		config[QStringLiteral("allowWeb")] =
			m_history->hasFlag(SessionHistory::AllowWeb);
		config[QStringLiteral("preferWebSockets")] =
			m_config->getConfigBool(config::PreferWebSockets);
	}
#endif
	addToHistory(net::ServerReply::makeSessionConf(config));
	emit sessionAttributeChanged(this);
}

void Session::sendUpdatedBanlist(bool clearForNonOperators)
{
	// Normal users don't get to see the actual IP addresses, but moderators and
	// local users do, since they have access to the server itself anyway.
	class LazyBanList {
	public:
		const net::Message &get(SessionHistory *history, bool sensitive)
		{
			if(sensitive) {
				if(m_sensitiveMsg.isNull()) {
					m_sensitiveMsg = make(history, true);
					// Theoretically the ban list could be so large that the
					// full information doesn't fit into a message, but the
					// condensed version does. Try to fall back in that case.
					if(m_sensitiveMsg.isNull()) {
						m_sensitiveMsg = get(history, false);
					}
				}
				return m_sensitiveMsg;
			} else {
				if(m_normalMsg.isNull()) {
					m_normalMsg = make(history, false);
				}
				return m_normalMsg;
			}
		}

		const net::Message &getBlank()
		{
			if(m_blankMsg.isNull()) {
				m_blankMsg = net::ServerReply::makeSessionConf(
					{{QStringLiteral("banlist"), QJsonArray()}});
			}
			return m_blankMsg;
		}

	private:
		net::Message make(SessionHistory *history, bool showSensitive)
		{
			return net::ServerReply::makeSessionConf(
				{{QStringLiteral("banlist"),
				  history->banlist().toJson(showSensitive)}});
		}

		net::Message m_normalMsg;
		net::Message m_sensitiveMsg;
		net::Message m_blankMsg;
	} banlist;

	for(Client *c : m_clients) {
		bool mod = c->isModerator();
		if(mod || c->isOperator()) {
			c->sendDirectMessage(
				banlist.get(m_history, mod || c->peerAddress().isLoopback()));
		} else if(clearForNonOperators) {
			c->sendDirectMessage(banlist.getBlank());
		}
	}
}

void Session::sendUpdatedAnnouncementList()
{
	// The announcement list is not usually included in the sessionconf.
	QJsonArray announcements;
	for(const sessionlisting::Announcement &a :
		m_announcements->getAnnouncements(this)) {
		announcements.append(QJsonObject{
			{QStringLiteral("url"), a.apiUrl.toString()},
		});
	}
	directToAll(net::ServerReply::makeSessionConf(
		{{QStringLiteral("announcements"), announcements}}));
}

void Session::sendUpdatedMuteList()
{
	// The mute list is not usually included in the sessionconf.
	QJsonArray muted;
	for(const Client *c : m_clients) {
		if(c->isMuted()) {
			muted.append(c->id());
		}
	}
	directToAll(
		net::ServerReply::makeSessionConf({{QStringLiteral("muted"), muted}}));
}

void Session::sendUpdatedAuthList()
{
	QHash<QString, bool> onlineAuthIds;
	for(Client *c : m_clients) {
		const QString &authId = c->authId();
		if(!authId.isEmpty()) {
			onlineAuthIds.insert(authId, c->isModerator());
		}
	}

	// The list of authenticated usernames can get so long that it doesn't fit
	// into a single message. Keep removing users to try and make it fit. We
	// also cap the number of "bare" offline authenticated users arbitrarily,
	// just to avoid this issue appearing in the first place.
	constexpr int MAX_BARE_OFFLINE_AUTH_USERS = 32;
	const QHash<QString, QString> &names = m_history->authenticatedUsernames();
	net::Message msg;
	int requiredLevel = 0;
	do {
		QJsonArray auth;
		int bareOfflineAuthUsers = 0;
		int effectiveRequiredLevel = requiredLevel;
		for(auto it = names.constBegin(); it != names.constEnd(); ++it) {
			const QString &authId = it.key();
			auto found = onlineAuthIds.constFind(authId);
			bool isOnline = found != onlineAuthIds.constEnd();
			bool isMod = isOnline && *found;
			bool isOp = m_history->isOperator(authId);
			bool isTrusted = m_history->isTrusted(authId);
			int level = isMod ? 3 : isOp ? 2 : isTrusted ? 1 : 0;
			if(isOnline || level >= effectiveRequiredLevel) {
				QJsonObject o = {
					{QStringLiteral("authId"), authId},
					{QStringLiteral("username"), it.value()},
				};
				if(isOp) {
					o[QStringLiteral("op")] = true;
				}
				if(isTrusted) {
					o[QStringLiteral("trusted")] = true;
				}
				if(isMod) {
					o[QStringLiteral("mod")] = true;
				}
				auth.append(o);

				if(level == 0 && !isOnline) {
					++bareOfflineAuthUsers;
					if(bareOfflineAuthUsers >= MAX_BARE_OFFLINE_AUTH_USERS) {
						effectiveRequiredLevel = qMax(1, requiredLevel);
					}
				}
			}
		}
		msg = net::ServerReply::makeSessionConf(
			QJsonObject{{QStringLiteral("auth"), auth}});
	} while(msg.isNull() && ++requiredLevel < 4);

	if(requiredLevel != 0) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::Status)
				.message(QStringLiteral("Auth list too large (level %1)")
							 .arg(requiredLevel)));
	}

	if(!msg.isNull()) {
		for(Client *c : m_clients) {
			if(c->isOperator()) {
				c->sendDirectMessage(msg);
			}
		}
	}
}

void Session::sendUpdatedInviteList()
{
	while(true) {
		net::Message msg = net::ServerReply::makeSessionConf(QJsonObject{
			{QStringLiteral("invitelist"), getInvitesDescription()}});
		if(!msg.isNull()) {
			for(Client *c : m_clients) {
				if(c->isOperator()) {
					c->sendDirectMessage(msg);
				}
			}
			break;
		}

		// Didn't fit, just delete the oldest invite and try again.
		QString oldestSecret;
		if(m_history->removeOldestInvite(&oldestSecret)) {
			log(Log()
					.about(Log::Level::Warn, Log::Topic::Invite)
					.message(QStringLiteral("Removed oldest invite %1 to fit "
											"them into a message")
								 .arg(oldestSecret)));
		} else {
			log(Log()
					.about(Log::Level::Warn, Log::Topic::Invite)
					.message(QStringLiteral(
						"Failed to fit invites into a message")));
			break;
		}
	}
}

void Session::handleClientMessage(Client &client, const net::Message &msg)
{
	// Filter away server-to-client-only messages
	switch(msg.type()) {
	case DP_MSG_JOIN:
	case DP_MSG_LEAVE:
	case DP_MSG_SOFT_RESET:
		client.log(
			Log()
				.about(Log::Level::Warn, Log::Topic::RuleBreak)
				.message(
					"Received server-to-user only command " + msg.typeName()));
		return;
	case DP_MSG_DISCONNECT:
		// we don't do anything with disconnect notifications from the client
		return;
	default:
		break;
	}

	// Some meta commands affect the server too
	switch(msg.type()) {
	case DP_MSG_SERVER_COMMAND: {
		net::ServerCommand cmd = net::ServerCommand::fromMessage(msg);
		handleClientServerCommand(&client, cmd.cmd, cmd.args, cmd.kwargs);
		return;
	}
	case DP_MSG_SESSION_OWNER: {
		if(!client.isOperator()) {
			client.log(Log()
						   .about(Log::Level::Warn, Log::Topic::RuleBreak)
						   .message("Tried to change session ownership"));
			return;
		}

		int count;
		const uint8_t *users =
			DP_msg_session_owner_users(msg.toSessionOwner(), &count);
		QVector<uint8_t> ids{users, users + count};
		ids.append(client.id());
		addClientMessage(
			client,
			net::makeSessionOwnerMessage(
				msg.contextId(), updateOwnership(ids, client.username())));
		return;
	}
	case DP_MSG_CHAT: {
		if(client.isMuted()) {
			return;
		}
		DP_MsgChat *mc = msg.toChat();
		bool bypass = DP_msg_chat_tflags(mc) & DP_MSG_CHAT_TFLAGS_BYPASS;
		if(client.isGhost()) {
			size_t len;
			const char *message = DP_msg_chat_message(mc, &len);
			QString s = QString::fromUtf8(message, compat::castSize(len));
			if(m_adminChat) {
				m_adminChat->addChatMessage(
					this, client.id(), client.peerAddress(), client.username(),
					s, DP_msg_chat_oflags(mc));
			}
			net::Message ghostMsg = net::ServerReply::makeAlert(s);
			if(bypass) {
				directToAll(ghostMsg);
			} else {
				addClientMessage(client, msg);
			}
			return;
		} else {
			if(m_adminChat) {
				size_t len;
				const char *message = DP_msg_chat_message(mc, &len);
				m_adminChat->addChatMessage(
					this, client.id(), client.peerAddress(), client.username(),
					QString::fromUtf8(message, compat::castSize(len)),
					DP_msg_chat_oflags(mc));
			}
			if(bypass) {
				chatMessageToAll(msg);
				return;
			}
		}
		break;
	}
	case DP_MSG_PRIVATE_CHAT: {
		DP_MsgPrivateChat *mpc = msg.toPrivateChat();
		uint8_t targetId = DP_msg_private_chat_target(mpc);
		if(client.isGhost()) {
			client.sendDirectMessage(net::makePrivateChatMessage(
				msg.contextId(), targetId, 0,
				QStringLiteral("Can't send private messages in ghost mode.")));
		} else if(targetId > 0) {
			Client *target = getClientById(targetId);
			if(target && !target->isAwaitingReset()) {
				client.sendDirectMessage(msg);
				target->sendDirectMessage(msg);
			}
		}
		return;
	}
	case DP_MSG_TRUSTED_USERS: {
		if(!client.isOperator()) {
			log(Log()
					.about(Log::Level::Warn, Log::Topic::RuleBreak)
					.message("Tried to change trusted user list"));
			return;
		}

		int count;
		const uint8_t *users =
			DP_msg_trusted_users_users(msg.toTrustedUsers(), &count);
		QVector<uint8_t> ids{users, users + count};
		addClientMessage(
			client,
			net::makeTrustedUsersMessage(
				msg.contextId(), updateTrustedUsers(ids, client.username())));
		return;
	}
	case DP_MSG_RESET_STREAM:
		onResetStream(client, msg);
		return;
	default:
		break;
	}

	// Rest of the messages are added to session history
	addClientMessage(client, msg);
}

void Session::addClientMessage(const Client &client, const net::Message &msg)
{
	if(initUserId() == client.id()) {
		addToInitStream(msg);
	} else {
		addToHistory(msg);
	}
}

void Session::addToInitStream(const net::Message &msg)
{
	Q_ASSERT(m_state != State::Running);

	if(m_state == State::Initialization) {
		addToHistory(msg);

	} else if(m_state == State::Reset) {
		m_resetstreamsize += int(msg.length());
		m_resetstream.append(msg);

		// Well behaved clients should be aware of the history limit and not
		// exceed it.
		if(m_history->sizeLimit() > 0 &&
		   m_resetstreamsize > m_history->sizeLimit()) {
			Client *resetter = getClientById(m_initUser);
			if(resetter)
				resetter->disconnectClient(
					Client::DisconnectionReason::Error,
					"History limit exceeded",
					QStringLiteral("%1 bytes").arg(m_resetstreamsize));
		}
	}
}

void Session::handleInitBegin(int ctxId)
{
	Client *c = getClientById(ctxId);
	if(!c) {
		// Shouldn't happen
		log(Log()
				.about(Log::Level::Error, Log::Topic::RuleBreak)
				.message(QString("Non-existent user %1 sent init-begin")
							 .arg(ctxId)));
		return;
	}

	if(ctxId != m_initUser) {
		c->log(Log()
				   .about(Log::Level::Warn, Log::Topic::RuleBreak)
				   .message(QString("Sent init-begin, but init user is #%1")
								.arg(m_initUser)));
		return;
	}

	c->log(Log()
			   .about(Log::Level::Debug, Log::Topic::Status)
			   .message("init-begin"));

	// It's possible that regular non-reset commands were still in the upload
	// buffer when the client started sending the reset snapshot. The init-begin
	// indicates the start of the true reset snapshot, so we can clear out the
	// buffer here. For backward-compatibility, sending the init-begin command
	// is optional.
	if(m_resetstreamsize > 0) {
		c->log(Log()
				   .about(Log::Level::Debug, Log::Topic::Status)
				   .message(
					   QStringLiteral("%1 extra messages cleared by init-begin")
						   .arg(m_resetstream.size())));
		m_resetstream.clear();
		m_resetstreamsize = 0;
	}
}

void Session::handleInitComplete(int ctxId)
{
	Client *c = getClientById(ctxId);
	if(!c) {
		// Shouldn't happen
		log(Log()
				.about(Log::Level::Error, Log::Topic::RuleBreak)
				.message(QString("Non-existent user %1 sent init-complete")
							 .arg(ctxId)));
		return;
	}

	if(ctxId != m_initUser) {
		c->log(Log()
				   .about(Log::Level::Warn, Log::Topic::RuleBreak)
				   .message(QString("Sent init-complete, but init user is #%1")
								.arg(m_initUser)));
		return;
	}

	c->log(Log()
			   .about(Log::Level::Debug, Log::Topic::Status)
			   .message("init-complete"));

	switchState(State::Running);
}


void Session::handleInitCancel(int ctxId)
{
	Client *c = getClientById(ctxId);
	if(!c) {
		// Shouldn't happen
		log(Log()
				.about(Log::Level::Error, Log::Topic::RuleBreak)
				.message(QString("Non-existent user %1 sent init-complete")
							 .arg(ctxId)));
		return;
	}

	if(ctxId != m_initUser) {
		c->log(Log()
				   .about(Log::Level::Warn, Log::Topic::RuleBreak)
				   .message(QString("Sent init-cancel, but init user is #%1")
								.arg(m_initUser)));
		return;
	}

	c->log(Log()
			   .about(Log::Level::Debug, Log::Topic::Status)
			   .message("init-cancel"));
	abortReset();
}

void Session::resetSession(int resetter, const QString &type)
{
	Q_ASSERT(m_state == State::Running);
	m_initUser = resetter;
	switchState(State::Reset);

	Client *client = getClientById(resetter);
	Q_ASSERT(client);
	QString name = client->username();
	QJsonObject params = {{QStringLiteral("name"), name}};
	if(type == QStringLiteral("current")) {
		directToAll(net::ServerReply::makeKeyAlertReset(
			QStringLiteral("%1 is compressing the canvas! Please wait, the "
						   "session should be available again shortly…")
				.arg(name),
			QStringLiteral("prepare"),
			net::ServerReply::KEY_RESET_PREPARE_CURRENT, params));
	} else if(type == QStringLiteral("past")) {
		directToAll(net::ServerReply::makeKeyAlertReset(
			QStringLiteral(
				"%1 is reverting the canvas to a previous state! Please wait, "
				"the session should be available again shortly…")
				.arg(name),
			QStringLiteral("prepare"), net::ServerReply::KEY_RESET_PREPARE_PAST,
			params));
	} else if(type == QStringLiteral("external")) {
		directToAll(net::ServerReply::makeKeyAlertReset(
			QStringLiteral("%1 is replacing the canvas! Please wait, the "
						   "session should be available again shortly…")
				.arg(name),
			QStringLiteral("prepare"),
			net::ServerReply::KEY_RESET_PREPARE_EXTERNAL, params));
	} else {
		directToAll(net::ServerReply::makeKeyAlertReset(
			QStringLiteral("Preparing for session reset by %1! Please wait, "
						   "the session should be available again shortly…")
				.arg(name),
			QStringLiteral("prepare"), net::ServerReply::KEY_RESET_PREPARE_BY,
			params));
	}

	client->sendDirectMessage(net::ServerReply::makeReset(
		QStringLiteral("Prepared to receive session data"),
		QStringLiteral("init")));
}

void Session::killSession(const QString &message, bool terminate)
{
	if(m_state == State::Shutdown)
		return;

	switchState(State::Shutdown);
	unlistAnnouncement(QUrl(), false);
	stopRecording();

	for(Client *c : m_clients) {
		c->disconnectClient(
			Client::DisconnectionReason::Shutdown, message,
			terminate ? QStringLiteral("terminate=true")
					  : QStringLiteral("terminate=false"));
		c->setSession(nullptr);
	}
	m_clients.clear();

	if(terminate)
		m_history->terminate();

	this->deleteLater();
}

void Session::directToAll(const net::Message &msg)
{
	for(Client *c : m_clients) {
		c->sendDirectMessage(msg);
	}
}

bool Session::messageAll(const QString &message, bool alert)
{
	if(!message.isEmpty()) {
		net::Message msg = alert ? net::ServerReply::makeAlert(message)
								 : net::ServerReply::makeMessage(message);
		if(!msg.isNull()) {
			directToAll(msg);
		}
		return true;
	}
	return false;
}

bool Session::keyMessageAll(
	const QString &message, bool alert, const QString &key,
	const QJsonObject &params)
{
	Q_ASSERT(!message.isEmpty());
	net::Message msg =
		alert ? net::ServerReply::makeKeyAlert(message, key, params)
			  : net::ServerReply::makeKeyMessage(message, key, params);
	if(msg.isNull()) {
		return false;
	} else {
		directToAll(msg);
		return true;
	}
}

void Session::ensureOperatorExists()
{
	// If there is a way to gain OP status without being explicitly granted,
	// it's OK for the session to not have any operators for a while.
	if(!m_history->opwordHash().isEmpty() ||
	   m_history->isAuthenticatedOperators()) {
		return;
	}

	const Client *firstClient = nullptr;
	for(const Client *c : m_clients) {
		if(!c->isGhost()) {
			if(c->isOperator()) {
				return;
			} else if(!firstClient) {
				firstClient = c;
			}
		}
	}

	// If we got here, there's no op and no way to gain the status via password
	// or by a registered user rejoining. Make the first client within reach op.
	if(firstClient) {
		changeOpStatus(firstClient->id(), true, QString());
	}
}

void Session::kickWebUsers(Client *by)
{
	QVector<Client *> clientsToKick;
	for(Client *c : m_clients) {
		if(c != by && !c->isModerator() && c->isBrowser() &&
		   !c->hasInviteSecret()) {
			clientsToKick.append(c);
		}
	}

	if(!clientsToKick.isEmpty()) {
		keyMessageAll(
			QStringLiteral(
				"Session password removed by %1. This server doesn't allow web "
				"browsers in public sessions, they will be disconnected.")
				.arg(
					by ? by->username()
					   : QStringLiteral("a server administrator")),
			true, net::ServerReply::KEY_KICK_WEB_USERS,
			by ? QJsonObject{{QStringLiteral("by"), by->username()}}
			   : QJsonObject());

		QString message =
			QStringLiteral("Web browser clients are only allowed on sessions "
						   "with a password, but %1 removed it.")
				.arg(
					by ? by->username()
					   : QStringLiteral("a server administrator"));
		for(Client *c : clientsToKick) {
			c->disconnectClient(
				Client::DisconnectionReason::Shutdown, message, QString());
		}
	}
}

void Session::addedToHistory(const net::Message &msg)
{
	if(m_recorder) {
		if(!DP_recorder_message_push_inc(m_recorder, msg.get())) {
			DP_recorder_free_join(m_recorder, nullptr);
			m_recorder = nullptr;
		}
	}
	m_lastEventTime.start();
	// TODO calculate activity score that can be shown in listings
}

void Session::restartRecording()
{
	if(m_recorder) {
		DP_recorder_free_join(m_recorder, nullptr);
	}

	// Start recording
	QString filename = utils::makeFilenameUnique(m_recordingFile, ".dprec");
	qDebug("Starting session recording %s", qPrintable(filename));

	DP_Output *output = DP_file_output_new_from_path(qUtf8Printable(filename));
	m_recorder =
		output
			? DP_recorder_new_inc(
				  DP_RECORDER_TYPE_BINARY,
				  DP_recorder_header_new(
					  "server-recording", "true", "version",
					  qUtf8Printable(m_history->protocolVersion().asString()),
					  static_cast<const char *>(nullptr)),
				  nullptr, nullptr, nullptr, output)
			: nullptr;
	if(!m_recorder) {
		qWarning(
			"Couldn't write session recording to %s: %s",
			qUtf8Printable(filename), DP_error());
		return;
	}

	int lastBatchIndex = 0;
	do {
		net::MessageList history;
		std::tie(history, lastBatchIndex) = m_history->getBatch(lastBatchIndex);
		for(const net::Message &msg : history) {
			DP_recorder_message_push_inc(m_recorder, msg.get());
		}

	} while(lastBatchIndex < m_history->lastIndex());
}

void Session::stopRecording()
{
	if(m_recorder) {
		DP_recorder_free_join(m_recorder, nullptr);
		m_recorder = nullptr;
	}
}

QString Session::uptime() const
{
	qint64 up = (QDateTime::currentMSecsSinceEpoch() -
				 m_history->startTime().toMSecsSinceEpoch()) /
				1000;

	int days = up / (60 * 60 * 24);
	up -= days * (60 * 60 * 24);

	int hours = up / (60 * 60);
	up -= hours * (60 * 60);

	int minutes = up / 60;

	QString uptime;
	if(days == 1)
		uptime = "one day, ";
	else if(days > 1)
		uptime = QString::number(days) + " days, ";

	if(hours == 1)
		uptime += "1 hour and ";
	else
		uptime += QString::number(hours) + " hours and ";

	if(minutes == 1)
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

void Session::makeAnnouncement(const QUrl &url)
{
	Q_ASSERT(m_announcements);
	m_announcements->announceSession(this, url);
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
	return sessionlisting::Session{
		m_config->internalConfig().localHostname,
		m_config->internalConfig().getAnnouncePort(),
		aliasOrId(),
		m_history->protocolVersion(),
		m_history->title(),
		userCount(),
		!m_history->passwordHash().isEmpty(),
		m_history->hasFlag(SessionHistory::Nsfm),
		m_history->founderName(),
		m_history->startTime(),
		m_history->maxUsers(),
		m_closed,
		activeDrawingUserCount(ACTIVE_THRESHOLD_MS),
		m_history->hasFlag(SessionHistory::AllowWeb),
		m_config->preferWebSockets(),
	};
}

bool Session::hasUrgentAnnouncementChange(
	const sessionlisting::Session &description) const
{
	return description.title != m_history->title() ||
		   description.nsfm != m_history->hasFlag(SessionHistory::Nsfm) ||
		   description.password != !m_history->passwordHash().isEmpty() ||
		   (description.users >= description.maxUsers) !=
			   (userCount() >= m_history->maxUsers()) ||
		   description.closed != m_closed ||
		   description.allowWeb != m_history->hasFlag(SessionHistory::AllowWeb);
}


void Session::onAnnouncementsChanged(const sessionlisting::Announcable *session)
{
	if(session == this)
		sendUpdatedAnnouncementList();
}

void Session::onAnnouncementError(
	const Announcable *session, const QString &message)
{
	if(session == this) {
		messageAll(message, false);
	}
}

void Session::onConfigValueChanged(const ConfigKey &key)
{
	if(key.index == config::IdleTimeLimit.index
#ifdef HAVE_WEBSOCKETS
	   || (m_config->internalConfig().webSocket &&
		   key.index == config::PreferWebSockets.index)
#endif
	) {
		sendUpdatedSessionProperties();
	} else if(key.index == config::ForceNsfm.index) {
		if(forceEnableNsfm(m_history, m_config)) {
			log(Log()
					.about(Log::Level::Info, Log::Topic::Status)
					.message("Forced NSFM after config change"));
		}
		sendUpdatedSessionProperties();
	} else if(key.index == config::AllowIdleOverride.index) {
		if(!m_config->getConfigBool(config::AllowIdleOverride)) {
			m_history->setFlag(SessionHistory::IdleOverride, false);
		}
		sendUpdatedSessionProperties();
	}
}

void Session::sendAbuseReport(
	const Client *reporter, int aboutUser, const QString &message)
{
	Q_ASSERT(reporter);

	reporter->log(
		Log()
			.about(Log::Level::Info, Log::Topic::Status)
			.message(QString("Abuse report about user %1 received: %2")
						 .arg(aboutUser)
						 .arg(message)));

	const QUrl url = m_config->internalConfig().reportUrl;
	if(!url.isValid()) {
		// This shouldn't happen normally. If the URL is not configured,
		// the server does not advertise the capability to receive reports.
		log(Log()
				.about(Log::Level::Warn, Log::Topic::Status)
				.message(
					"Cannot send abuse report: server URL not configured!"));
		return;
	}

	if(!m_config->getConfigBool(config::AbuseReport)) {
		// This can happen if reporting is disabled when a session is still in
		// progress
		log(Log()
				.about(Log::Level::Warn, Log::Topic::Status)
				.message("Cannot send abuse report: not enabled!"));
		return;
	}

	QJsonObject o;
	o["session"] = id();
	o["sessionTitle"] = m_history->title();
	o["user"] = reporter->username();
	o["auth"] = reporter->isAuthenticated();
	o["ip"] = reporter->peerAddress().toString();
	if(aboutUser > 0)
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
	QNetworkReply *reply =
		networkaccess::getInstance()->post(req, QJsonDocument(o).toJson());
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		if(reply->error() != QNetworkReply::NoError) {
			log(Log()
					.about(Log::Level::Warn, Log::Topic::Status)
					.message(
						"Unable to send abuse report: " +
						reply->errorString()));
		}
	});
	connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

QJsonObject Session::getDescription(bool full, bool invite) const
{
	// The basic description contains just the information
	// needed for the login session listing
	QJsonObject o{
		{QStringLiteral("id"), id()},
		{QStringLiteral("alias"), idAlias()},
		{QStringLiteral("protocol"), m_history->protocolVersion().asString()},
		{QStringLiteral("userCount"), userCount()},
		{QStringLiteral("activeDrawingUserCount"),
		 activeDrawingUserCount(ACTIVE_THRESHOLD_MS)},
		{QStringLiteral("maxUserCount"), m_history->maxUsers()},
		{QStringLiteral("founder"), m_history->founderName()},
		{QStringLiteral("title"), m_history->title()},
		{QStringLiteral("autotitle"),
		 m_history->hasFlag(SessionHistory::AutoTitle)},
		{QStringLiteral("hasPassword"),
		 !invite && !m_history->passwordHash().isEmpty()},
		{QStringLiteral("closed"), isClosed(invite)},
		{QStringLiteral("authOnly"),
		 !invite && m_history->hasFlag(SessionHistory::AuthOnly)},
		{QStringLiteral("nsfm"), m_history->hasFlag(SessionHistory::Nsfm)},
		{QStringLiteral("startTime"),
		 m_history->startTime().toString(Qt::ISODate)},
		{QStringLiteral("size"), int(m_history->sizeInBytes())},
		{QStringLiteral("persistent"),
		 m_history->hasFlag(SessionHistory::Persistent)},
		{QStringLiteral("invites"),
		 m_history->hasFlag(SessionHistory::Invites)},
	};

	if(m_config->getConfigBool(config::AllowIdleOverride)) {
		o[QStringLiteral("idleOverride")] =
			m_history->hasFlag(SessionHistory::IdleOverride);
	}

#ifdef HAVE_WEBSOCKETS
	if(m_config->internalConfig().webSocket) {
		o[QStringLiteral("allowWeb")] =
			invite || m_history->hasFlag(SessionHistory::AllowWeb);
		if(m_config->getConfigBool(config::PreferWebSockets)) {
			o.insert(QStringLiteral("preferWebSockets"), true);
		}
	}
#endif

	if(full) {
		// Full descriptions includes detailed info for server admins.
		o[QStringLiteral("maxSize")] = int(m_history->sizeLimit());
		o[QStringLiteral("resetThreshold")] =
			int(m_history->autoResetThreshold());
		o[QStringLiteral("effectiveResetThreshold")] =
			int(m_history->effectiveAutoResetThreshold());
		o[QStringLiteral("deputies")] =
			m_history->hasFlag(SessionHistory::Deputies);
		o[QStringLiteral("hasOpword")] = !m_history->opwordHash().isEmpty();

		QJsonArray users;
		for(const Client *user : m_clients) {
			users.append(getUserDescription(user));
		}
		for(auto u = m_pastClients.constBegin(); u != m_pastClients.constEnd();
			++u) {
			users << QJsonObject{
				{QStringLiteral("id"), u->id},
				{QStringLiteral("name"), u->username},
				{QStringLiteral("ip"), u->peerAddress.toString()},
				{QStringLiteral("s"), u->sid},
				{QStringLiteral("online"), false}};
		}
		o[QStringLiteral("users")] = users;
		o[QStringLiteral("listings")] = getListingsDescription();
		o[QStringLiteral("invitelist")] = getInvitesDescription(true);
		if(m_adminChat) {
			o[QStringLiteral("chat")] = m_adminChat->getDescription();
		} else {
			// Having the "chat" key present indicates we support that feature.
			o[QStringLiteral("chat")] = QJsonValue();
		}
	}

	return o;
}

QJsonArray Session::getListingsDescription() const
{
	QJsonArray listings;
	const auto announcements = m_announcements->getAnnouncements(this);
	for(const sessionlisting::Announcement &a : announcements) {
		listings << QJsonObject{
			{"id", a.listingId},
			{"url", a.apiUrl.toString()},
		};
	}
	return listings;
}

QJsonObject Session::getUserDescription(const Client *user) const
{
	QJsonObject u = user->description(false);
	u[QStringLiteral("online")] = true;
	u[QStringLiteral("holdLocked")] = user->isHoldLocked();

	Client::ResetFlags resetFlags = user->resetFlags();
	QJsonArray f;
	if(resetFlags.testFlag(Client::ResetFlag::Awaiting)) {
		f.append(QStringLiteral("awaiting"));
	}
	if(resetFlags.testFlag(Client::ResetFlag::Queried)) {
		f.append(QStringLiteral("queried"));
	}
	if(resetFlags.testFlag(Client::ResetFlag::Responded)) {
		f.append(QStringLiteral("responded"));
	}
	if(resetFlags.testFlag(Client::ResetFlag::Streaming)) {
		f.append(QStringLiteral("streaming"));
	}
	u[QStringLiteral("resetFlags")] = f;

	return u;
}

QJsonArray Session::getInvitesDescription(bool full) const
{
	QJsonArray invites;
	const QHash<QString, Invite> &invitesBySecret =
		m_history->invitesBySecret();
	for(QHash<QString, Invite>::const_iterator
			it = invitesBySecret.constBegin(),
			end = invitesBySecret.constEnd();
		it != end; ++it) {
		invites.append(it->toJson(full));
	}
	return invites;
}

QJsonObject Session::getExportBanList() const
{
	return m_history->banlist().toExportJson();
}

bool Session::importBans(
	const QJsonObject &data, int &outTotal, int &outImported,
	const Client *client)
{
	bool ok = m_history->importBans(data, outTotal, outImported, client);
	if(ok && outImported > 0) {
		sendUpdatedBanlist();
	}
	return ok;
}

JsonApiResult Session::callJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request,
	bool sectionLocked)
{
	if(!path.isEmpty()) {
		QString head;
		QStringList tail;
		std::tie(head, tail) = popApiPath(path);

		if(head == QStringLiteral("listing")) {
			return callListingsJsonApi(method, tail, request);
		} else if(head == QStringLiteral("auth")) {
			return callAuthJsonApi(method, tail, request);
		} else if(head == QStringLiteral("chat")) {
			return callChatJsonApi(method, tail, request);
		} else if(head == QStringLiteral("invites")) {
			return callInvitesJsonApi(method, tail, request);
		}

		int userId = head.toInt();
		if(userId > 0) {
			Client *c = getClientById(userId);
			if(c)
				return c->callJsonApi(method, tail, request);
		}

		return JsonApiNotFound();
	}

	if(method == JsonApiMethod::Update) {
		setSessionConfig(request, nullptr);

		if(request.contains(QStringLiteral("message"))) {
			QString message = request[QStringLiteral("message")].toString();
			bool sent = messageAll(message, false);
			if(!sent) {
				return JsonApiErrorResult(
					JsonApiResult::BadRequest,
					QStringLiteral("Message too long"));
			} else if(m_adminChat) {
				m_adminChat->addAdminMessage(this, message);
			}
		}

		if(request.contains(QStringLiteral("alert"))) {
			QString message = request[QStringLiteral("alert")].toString();
			bool sent = messageAll(message, true);
			if(!sent) {
				return JsonApiErrorResult(
					JsonApiResult::BadRequest,
					QStringLiteral("Alert too long"));
			} else if(m_adminChat) {
				m_adminChat->addAdminMessage(this, message);
			}
		}

	} else if(method == JsonApiMethod::Delete) {
		QString reason = request[QStringLiteral("reason")].toString().trimmed();
		if(!reason.isEmpty()) {
			keyMessageAll(
				QStringLiteral("Session shut down by administrator: %1")
					.arg(reason),
				true, net::ServerReply::KEY_TERMINATE_SESSION_ADMIN,
				{{QStringLiteral("reason"), reason}});
		}
		killSession(QStringLiteral("Session terminated by administrator"));
		return JsonApiResult{
			JsonApiResult::Ok, QJsonDocument(QJsonObject{{"status", "ok"}})};
	}

	QJsonObject body = getDescription(true);
	body.insert(QStringLiteral("_locked"), sectionLocked);
	return JsonApiResult{JsonApiResult::Ok, QJsonDocument(body)};
}

JsonApiResult Session::callListingsJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	int pathLength = path.length();
	if(pathLength > 1) {
		return JsonApiNotFound();
	}

	if(pathLength == 1) {
		const int id = path.at(0).toInt();

		const auto announcements = m_announcements->getAnnouncements(this);
		for(const sessionlisting::Announcement &a : announcements) {
			if(a.listingId == id) {
				if(method == JsonApiMethod::Delete) {
					unlistAnnouncement(a.apiUrl.toString());
					return JsonApiResult{
						JsonApiResult::Ok,
						QJsonDocument(QJsonObject{{"status", "ok"}})};

				} else {
					return JsonApiBadMethod();
				}
			}
		}

		return JsonApiNotFound();
	}

	if(method == JsonApiMethod::Get) {
		return JsonApiResult{
			JsonApiResult::Ok, QJsonDocument(getListingsDescription())};
	} else if(method == JsonApiMethod::Create) {
		if(request.contains("url")) {
			makeAnnouncement(QUrl(request["url"].toString()));
			return JsonApiResult{
				JsonApiResult::Ok,
				QJsonDocument(QJsonObject{{"status", "ok"}})};
		} else {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, QStringLiteral("Missing url"));
		}
	} else {
		return JsonApiBadMethod();
	}
}

JsonApiResult Session::callAuthJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	if(method != JsonApiMethod::Create)
		return JsonApiBadMethod();

	int pathLength = path.length();

	if(pathLength > 1)
		return JsonApiNotFound();

	if(pathLength == 1) {
		const QString head = path.at(0);

		if(head == "op") {
			if(request.contains("password")) {
				bool status = passwordhash::check(
					request["password"].toString(), m_history->opwordHash());
				return JsonApiResult{
					JsonApiResult::Ok,
					QJsonDocument(QJsonObject{{"status", status}})};
			}
		}

		return JsonApiNotFound();
	}

	if(request.contains("password")) {
		bool status = m_history->checkPassword(request["password"].toString());
		return JsonApiResult{
			JsonApiResult::Ok, QJsonDocument(QJsonObject{{"status", status}})};
	}

	return JsonApiNotFound();
}

JsonApiResult Session::callChatJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	if(!path.isEmpty()) {
		return JsonApiNotFound();
	}

	if(method == JsonApiMethod::Get) {
		if(!m_adminChat) {
			return JsonApiErrorResult(
				JsonApiResult::NotFound, QStringLiteral("Chat not connected"));
		}

		m_adminChat->restartTimer();

		int offset = parseRequestInt(request, QStringLiteral("offset"), 0, -1);
		if(offset < 0) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, QStringLiteral("Invalid offset"));
		}

		QJsonArray messages = m_adminChat->getMessages(&offset);
		return JsonApiResult{
			JsonApiResult::Ok, QJsonDocument(QJsonObject{
								   {QStringLiteral("offset"), offset},
								   {QStringLiteral("messages"), messages},
							   })};

	} else if(method == JsonApiMethod::Create) {
		if(m_adminChat) {
			m_adminChat->restartTimer();
			return JsonApiErrorResult(
				JsonApiResult::Conflict,
				QStringLiteral("Chat already connected"));
		}

		QString message =
			request[QStringLiteral("message")].toString().trimmed();
		if(message.isEmpty()) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, QStringLiteral("Missing message"));
		}

		constexpr int MIN_TIMEOUT = 30000;		// 30 seconds
		constexpr int MAX_TIMEOUT = 900000;		// 15 minutes
		constexpr int DEFAULT_TIMEOUT = 300000; // 5 minutes
		int timeoutMs = parseRequestInt(
			request, QStringLiteral("timeout"), DEFAULT_TIMEOUT, -1);
		if(timeoutMs < MIN_TIMEOUT) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest,
				QStringLiteral("Timeout too short, must be >= %1 ms")
					.arg(MIN_TIMEOUT));
		} else if(timeoutMs > MAX_TIMEOUT) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest,
				QStringLiteral("Timeout too long, must be <= %1 ms")
					.arg(MAX_TIMEOUT));
		}

		bool sent = messageAll(message, true);
		if(!sent) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, QStringLiteral("Message too long"));
		}

		log(Log()
				.about(Log::Level::Info, Log::Topic::Join)
				.message(QStringLiteral("Administrator connected to chat")));
		m_adminChat = new AdminChat(this, timeoutMs);
		m_adminChat->addAdminMessage(this, message);

		int offset = 0;
		QJsonArray messages = m_adminChat->getMessages(&offset);
		return JsonApiResult{
			JsonApiResult::Ok, QJsonDocument(QJsonObject{
								   {QStringLiteral("offset"), offset},
								   {QStringLiteral("messages"), messages},
							   })};

	} else if(method == JsonApiMethod::Update) {
		if(!m_adminChat) {
			return JsonApiErrorResult(
				JsonApiResult::NotFound, QStringLiteral("Chat not connected"));
		}

		m_adminChat->restartTimer();

		QString message =
			request[QStringLiteral("message")].toString().trimmed();
		if(message.isEmpty()) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, QStringLiteral("Missing message"));
		}

		int offset = parseRequestInt(request, QStringLiteral("offset"), 0, -1);
		if(offset < 0) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, QStringLiteral("Invalid offset"));
		}

		bool sent = messageAll(message, true);
		if(!sent) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, QStringLiteral("Message too long"));
		}

		m_adminChat->addAdminMessage(this, message);

		QJsonArray messages = m_adminChat->getMessages(&offset);
		return JsonApiResult{
			JsonApiResult::Ok, QJsonDocument(QJsonObject{
								   {QStringLiteral("offset"), offset},
								   {QStringLiteral("messages"), messages},
							   })};

	} else if(method == JsonApiMethod::Delete) {
		if(!m_adminChat) {
			return JsonApiErrorResult(
				JsonApiResult::NotFound, QStringLiteral("Chat not connected"));
		}

		QString message =
			request[QStringLiteral("message")].toString().trimmed();
		if(!message.isEmpty()) {
			bool sent = messageAll(message, true);
			if(!sent) {
				return JsonApiErrorResult(
					JsonApiResult::BadRequest,
					QStringLiteral("Message too long"));
			}
			m_adminChat->addAdminMessage(this, message);
		}

		int offset = parseRequestInt(request, QStringLiteral("offset"), 0, -1);
		if(offset < 0) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, QStringLiteral("Invalid offset"));
		}

		log(Log()
				.about(Log::Level::Info, Log::Topic::Leave)
				.message(
					QStringLiteral("Administrator disconnected from chat")));
		QJsonArray messages = m_adminChat->getMessages(&offset);
		delete m_adminChat;
		m_adminChat = nullptr;

		return JsonApiResult{
			JsonApiResult::Ok, QJsonDocument(QJsonObject{
								   {QStringLiteral("offset"), offset},
								   {QStringLiteral("messages"), messages},
							   })};

	} else {
		return JsonApiBadMethod();
	}
}

JsonApiResult Session::callInvitesJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	int pathCount = path.size();
	if(pathCount == 0) {
		if(method == JsonApiMethod::Get) {
			return JsonApiResult{
				JsonApiResult::Ok,
				QJsonDocument(QJsonObject{
					{QStringLiteral("invites"), getInvitesDescription(true)}})};
		} else if(method == JsonApiMethod::Create) {
			int maxUses =
				parseRequestInt(request, QStringLiteral("maxUses"), 0, 0);
			if(maxUses < 1) {
				return JsonApiErrorResult(
					JsonApiResult::BadRequest,
					QStringLiteral("Missing or invalid maxUses"));
			}

			bool trust =
				parseRequestBool(request, QStringLiteral("trust"), 0, 0);
			bool op = parseRequestBool(request, QStringLiteral("op"), 0, 0);
			Invite *invite = createInvite(nullptr, maxUses, trust, op);
			if(invite) {
				return JsonApiResult{
					JsonApiResult::Ok, QJsonDocument(invite->toJson())};
			} else {
				return JsonApiErrorResult(
					JsonApiResult::BadRequest,
					QStringLiteral("Too many invites"));
			}
		} else {
			return JsonApiBadMethod();
		}
	} else if(pathCount == 1) {
		QString secret = path[0];
		if(method == JsonApiMethod::Get) {
			const QHash<QString, Invite> &invitesBySecret =
				m_history->invitesBySecret();
			QHash<QString, Invite>::const_iterator it =
				invitesBySecret.constFind(secret);
			if(it == invitesBySecret.constEnd()) {
				return JsonApiNotFound();
			} else {
				return JsonApiResult{
					JsonApiResult::Ok, QJsonDocument(it->toJson())};
			}
		} else if(method == JsonApiMethod::Delete) {
			if(removeInvite(nullptr, secret)) {
				return JsonApiResult{
					JsonApiResult::Ok,
					QJsonDocument(QJsonObject{
						{QStringLiteral("status"), QStringLiteral("ok")}})};
			} else {
				return JsonApiNotFound();
			}
		} else {
			return JsonApiBadMethod();
		}
	} else {
		return JsonApiNotFound();
	}
}

void Session::timeOutAdminChat()
{
	if(m_adminChat) {
		log(Log()
				.about(Log::Level::Info, Log::Topic::Leave)
				.message(QStringLiteral(
					"Administrator disconnected from chat (timeout)")));
		delete m_adminChat;
		m_adminChat = nullptr;
	}
}

void Session::log(const Log &log)
{
	Log entry = log;
	entry.session(id());
	m_config->logger()->logMessage(entry);

	if(entry.level() < Log::Level::Debug) {
		net::Message msg = makeLogMessage(entry);
		if(entry.isSensitive()) {
			for(Client *c : m_clients) {
				if(c->isModerator()) {
					c->sendDirectMessage(msg);
				}
			}
		} else {
			directToAll(msg);
		}
	}
}

void Session::logFor(Client *client, const Log &entry)
{
	if(client) {
		client->log(entry);
	} else {
		log(entry);
	}
}

}
