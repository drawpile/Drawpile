/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2017 Calle Laakkonen

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

#include "session.h"
#include "client.h"
#include "serverconfig.h"
#include "inmemoryhistory.h"
#include "serverlog.h"

#include "../net/control.h"
#include "../net/meta.h"
#include "../record/writer.h"
#include "../util/filename.h"
#include "../util/passwordhash.h"

#include "config.h"

#include <QTimer>

namespace server {

using protocol::MessagePtr;

Session::Session(SessionHistory *history, ServerConfig *config, QObject *parent)
	: QObject(parent),
	m_config(config),
	m_state(Initialization),
	m_initUser(-1),
	m_recorder(nullptr),
	m_history(history),
	m_resetstreamsize(0),
	m_publicListingClient(nullptr),
	m_lastEventTime(QDateTime::currentDateTime()),
	m_closed(false),
	m_historyLimitWarningSent(false)
{
	m_history->setParent(this);
	m_history->setSizeLimit(config->getConfigSize(config::SessionSizeLimit));
	m_historyLimitWarning = m_history->sizeLimit() * 0.7;

	if(history->sizeInBytes()>0)
		m_state = Running;

	for(const QString &announcement : m_history->announcements())
		makeAnnouncement(QUrl(announcement));
}

QString Session::toLogString() const {
	if(idAlias().isEmpty())
		return QStringLiteral("Session %1:").arg(id().toString());
	else
		return QStringLiteral("Session %1|%2:").arg(id().toString(), idAlias());
}

void Session::switchState(State newstate)
{
	if(newstate==Initialization) {
		qFatal("Illegal state change to Initialization from %d", m_state);

	} else if(newstate==Running) {
		if(m_state!=Initialization && m_state!=Reset)
			qFatal("Illegal state change to Running from %d", m_state);

		m_initUser = -1;
		bool success = true;

		if(m_state==Reset && !m_resetstream.isEmpty()) {
			// Reset buffer uploaded. Now perform the reset before returning to
			// normal running state.

			// Add list of currently logged in users to reset snapshot
			QList<uint8_t> owners;
			for(const Client *c : m_clients) {
				m_resetstream.prepend(c->joinMessage());
				if(c->isOperator())
					owners << c->id();
			}
			m_resetstream.prepend(protocol::MessagePtr(new protocol::SessionOwner(0, owners)));

			// Send reset snapshot
			if(!m_history->reset(m_resetstream)) {
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

				m_historyLimitWarningSent = false;

				sendUpdatedSessionProperties();
			}

			m_resetstream.clear();
			m_resetstreamsize = 0;
		}

		if(success && !m_recordingFile.isEmpty())
			restartRecording();

		for(Client *c : m_clients)
			c->enqueueHeldCommands();

	} else if(newstate==Reset) {
		if(m_state!=Running)
			qFatal("Illegal state change to Reset from %d", m_state);

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
	connect(history(), &SessionHistory::newMessagesAvailable, user, &Client::sendNextHistoryBatch);

	if(host) {
		Q_ASSERT(m_state == Initialization);
		m_initUser = user->id();
	} else {
		// Notify the client how many messages to expect (at least)
		// The client can use this information to display a progress bar during the login phase
		protocol::ServerReply catchup;
		catchup.type = protocol::ServerReply::CATCHUP;
		catchup.reply["count"] = m_history->lastIndex() - m_history->firstIndex();
		user->sendDirectMessage(protocol::MessagePtr(new protocol::Command(0, catchup)));
	}

	const QString welcomeMessage = m_config->getConfigString(config::WelcomeMessage);
	if(!welcomeMessage.isEmpty()) {
		user->sendSystemChat(welcomeMessage);
	}

	addToHistory(user->joinMessage());

	ensureOperatorExists();

	// Let new users know about the size limit too
	if(m_historyLimitWarning > 0 && m_history->sizeInBytes() > m_historyLimitWarning) {
		protocol::ServerReply warning;
		warning.type = protocol::ServerReply::SIZELIMITWARNING;
		warning.reply["size"] = int(m_history->sizeInBytes());
		warning.reply["maxSize"] = int(m_historyLimitWarning);
		user->sendDirectMessage(protocol::MessagePtr(new protocol::Command(0, warning)));
	}

	// Make sure everyone is up to date
	sendUpdatedAnnouncementList();
	sendUpdatedBanlist();
	sendUpdatedMuteList();

	m_history->idQueue().setIdForName(user->id(), user->username());

	user->log(Log().about(Log::Level::Info, Log::Topic::Join).message("Joined session"));
	emit userConnected(this, user);
}

void Session::removeUser(Client *user)
{
	if(!m_clients.removeOne(user))
		return;

	user->log(Log().about(Log::Level::Info, Log::Topic::Leave).message("Left session"));

	disconnect(user, &Client::loggedOff, this, &Session::removeUser);
	disconnect(m_history, &SessionHistory::newMessagesAvailable, user, &Client::sendNextHistoryBatch);

	if(user->id() == m_initUser && m_state == Reset) {
		// Whoops, the resetter left before the job was done!
		// We simply cancel the reset in that case and go on
		m_initUser = -1;
		m_resetstream.clear();
		m_resetstreamsize = 0;
		switchState(Running);
		messageAll("Session reset cancelled.", true);
	}

	addToHistory(MessagePtr(new protocol::UserLeave(user->id())));
	m_history->idQueue().reserveId(user->id()); // Try not to reuse the ID right away

	ensureOperatorExists();

	// Reopen the session when the last user leaves
	if(m_clients.isEmpty()) {
		setClosed(false);
	}

	historyCacheCleanup();

	emit userDisconnected(this);
}

Client *Session::getClientById(int id)
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
	if(m_history->addBan(target->username(), target->peerAddress(), bannedBy)) {
		target->log(Log().about(Log::Level::Info, Log::Topic::Ban).message("Banned by " + bannedBy));
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

void Session::setClosed(bool closed)
{
	if(m_closed != closed) {
		m_closed = closed;
		sendUpdatedSessionProperties();
	}
}

// In Qt 5.7 we can just use Flags.setFlag(flag, true/false);
// Remove this once we can drop support for older Qt versions
template<class F, class Ff> static void setFlag(F &flags, Ff f, bool set)
{
	if(set)
		flags |= f;
	else
		flags &= ~f;
}

void Session::setSessionConfig(const QJsonObject &conf)
{
	bool changed = false;

	if(conf.contains("closed")) {
		m_closed = conf["closed"].toBool();
		changed = true;
	}

	SessionHistory::Flags flags = m_history->flags();
	const SessionHistory::Flags oldFlags = flags;

	if(conf.contains("persistent")) {
		setFlag(flags, SessionHistory::Persistent, conf["persistent"].toBool() && m_config->getConfigBool(config::EnablePersistence));
	}

	if(conf.contains("title")) {
		m_history->setTitle(conf["title"].toString().mid(0, 100));
		changed = true;
	}

	if(conf.contains("maxUserCount")) {
		m_history->setMaxUsers(conf["maxUserCount"].toInt());
		changed = true;
	}

	if(conf.contains("password")) {
		m_history->setPassword(conf["password"].toString());
		changed = true;
	}

	if(conf.contains("opword")) {
		m_history->setOpword(conf["opword"].toString());
		changed = true;
	}

	// Note: this bit is only relayed by the server: it informs
	// the client whether to send preserved/recorded chat messages
	// by default.
	if(conf.contains("preserveChat")) {
		setFlag(flags, SessionHistory::PreserveChat, conf["preserveChat"].toBool());
	}

	if(conf.contains("nsfm")) {
		setFlag(flags, SessionHistory::Nsfm, conf["nsfm"].toBool());
	}

	m_history->setFlags(flags);
	changed |= oldFlags != flags;

	if(changed)
		sendUpdatedSessionProperties();
}

bool Session::checkPassword(const QString &password) const
{
	return passwordhash::check(password, m_history->passwordHash());
}

QList<uint8_t> Session::updateOwnership(QList<uint8_t> ids, const QString &changedBy)
{
	QList<uint8_t> truelist;
	for(Client *c : m_clients) {
		bool op = ids.contains(c->id()) | c->isModerator();
		if(op != c->isOperator()) {
			c->setOperator(op);
			if(op)
				c->log(Log().about(Log::Level::Info, Log::Topic::Op).message("Made operator by " + changedBy));
			else
				c->log(Log().about(Log::Level::Info, Log::Topic::Deop).message("Operator status revoked by " + changedBy));
		}
		if(c->isOperator())
			truelist << c->id();
	}
	return truelist;
}

void Session::changeOpStatus(int id, bool op, const QString &changedBy)
{
	QList<uint8_t> ids;
	for(Client *c : m_clients) {
		if(c->id() == id && c->isOperator() != op) {
			c->setOperator(op);
			if(op)
				c->log(Log().about(Log::Level::Info, Log::Topic::Op).message("Made operator by " + changedBy));
			else
				c->log(Log().about(Log::Level::Info, Log::Topic::Deop).message("Operator status revoked by " + changedBy));
		}

		if(c->isOperator())
			ids << c->id();
	}

	addToHistory(protocol::MessagePtr(new protocol::SessionOwner(0, ids)));
}

void Session::sendUpdatedSessionProperties()
{
	protocol::ServerReply props;
	props.type = protocol::ServerReply::SESSIONCONF;
	QJsonObject	conf;
	conf["closed"] = m_closed; // this refers specifically to the closed flag, not the general status
	conf["persistent"] = isPersistent();
	conf["title"] = title();
	conf["maxUserCount"] = m_history->maxUsers();
	conf["preserveChat"] = m_history->flags().testFlag(SessionHistory::PreserveChat);
	conf["nsfm"] = m_history->flags().testFlag(SessionHistory::Nsfm);
	conf["hasPassword"] = hasPassword();
	conf["hasOpword"] = hasOpword();
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
	conf["banlist"] = banlist().toJson(false);
	msg.reply["config"] = conf;

	// Normal users don't get to see the actual IP addresses
	protocol::MessagePtr normalVersion(new protocol::Command(0, msg));

	// But moderators and local users do
	conf["banlist"] = banlist().toJson(true);
	msg.reply["config"] = conf;
	protocol::MessagePtr modVersion(new protocol::Command(0, msg));

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
	for(const sessionlisting::Announcement &a : announcements()) {
		list.append(a.apiUrl.toString());
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

void Session::addToHistory(const protocol::MessagePtr &msg)
{
	if(m_state == Shutdown)
		return;

	// Add message to history (if there is space)
	if(!m_history->addMessage(msg)) {
		const Client *shame = getClientById(msg->contextId());
		messageAll("History size limit reached!", false);
		messageAll((shame ? shame->username() : QString("user #%1").arg(msg->contextId())) + " broke the camel's back. Session must be reset to continue drawing.", false);
		return;
	}


	// The hosting user must skip the history uploaded during initialization
	// (since they originated it), but we still want to send them notifications.
	if(m_state == Initialization) {
		Client *origin = getClientById(m_initUser);
		Q_ASSERT(origin);
		if(origin) {
			origin->setHistoryPosition(m_history->lastIndex());
			if(!msg->isCommand())
				origin->sendDirectMessage(msg);
		}
	}

	// Add message to recording
	if(m_recorder)
		m_recorder->recordMessage(msg);
	m_lastEventTime = QDateTime::currentDateTime();

	// Send a warning if approaching size limit.
	// The clients should update their internal size limits and reset the session when necessary.
	if(m_historyLimitWarning>0 && !m_historyLimitWarningSent && m_history->sizeInBytes() > m_historyLimitWarning) {
		log(Log().about(Log::Level::Warn, Log::Topic::Status).message("History limit warning treshold reached."));

		protocol::ServerReply warning;
		warning.type = protocol::ServerReply::SIZELIMITWARNING;
		warning.reply["size"] = int(m_history->sizeInBytes());
		warning.reply["maxSize"] = int(m_historyLimitWarning);

		directToAll(protocol::MessagePtr(new protocol::Command(0, warning)));
		m_historyLimitWarningSent = true;
	}
}

void Session::addToInitStream(protocol::MessagePtr msg)
{
	Q_ASSERT(m_state == Initialization || m_state == Reset || m_state == Shutdown);

	if(m_state == Initialization) {
		addToHistory(msg);

	} else if(m_state == Reset) {
		m_resetstreamsize += msg->length();
		m_resetstream.append(msg);

		// Well behaved clients should be aware of the history limit and not exceed it.
		if(m_history->sizeLimit()>0 && m_resetstreamsize > m_history->sizeLimit()) {
			Client *resetter = getClientById(m_initUser);
			if(resetter)
				resetter->disconnectError("History limit exceeded");
		}
	}
}

void Session::handleInitComplete(int ctxId)
{
	Client *c = getClientById(ctxId);
	if(!c) {
		// Shouldn't happen
		c->log(Log().about(Log::Level::Error, Log::Topic::RuleBreak).message(QString("Non-existent user %1 sent init-complete").arg(ctxId)));
		return;
	}

	if(ctxId != m_initUser) {
		c->log(Log().about(Log::Level::Warn, Log::Topic::RuleBreak).message(QString("Sent init-complete, but init user is #%1").arg(m_initUser)));
		return;
	}

	c->log(Log().about(Log::Level::Debug, Log::Topic::Status).message("init-complete"));

	switchState(Running);
}

void Session::resetSession(int resetter)
{
	Q_ASSERT(m_state == Running);
	Q_ASSERT(getClientById(resetter));

	m_initUser = resetter;
	switchState(Reset);

	protocol::ServerReply resetRequest;
	resetRequest.type = protocol::ServerReply::RESET;
	resetRequest.reply["state"] = "init";
	resetRequest.message = "Prepared to receive session data";

	getClientById(resetter)->sendDirectMessage(protocol::MessagePtr(new protocol::Command(0, resetRequest)));
}

void Session::killSession(bool terminate)
{
	if(m_state == Shutdown)
		return;

	switchState(Shutdown);
	unlistAnnouncement("*", false);
	stopRecording();

	for(Client *c : m_clients)
		c->disconnectShutdown();
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
		QList<protocol::MessagePtr> history;
		std::tie(history, lastBatchIndex) = m_history->getBatch(lastBatchIndex);
		for(const MessagePtr &m : history)
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

sessionlisting::AnnouncementApi *Session::publicListingClient()
{
	if(!m_publicListingClient) {
		m_publicListingClient = new sessionlisting::AnnouncementApi(this);
		m_publicListingClient->setLocalAddress(m_config->getConfigString(config::LocalAddress));
		connect(m_publicListingClient, &sessionlisting::AnnouncementApi::sessionAnnounced, this, &Session::sessionAnnounced);
		connect(m_publicListingClient, &sessionlisting::AnnouncementApi::error, this, &Session::sessionAnnouncementError);
		connect(m_publicListingClient, &sessionlisting::AnnouncementApi::messageReceived, this, [this](const QString &message) {
			this->messageAll(message, false);
		});
		connect(m_publicListingClient, &sessionlisting::AnnouncementApi::logMessage, this, &Session::log);

		QTimer *refreshTimer = new QTimer(this);
		connect(refreshTimer, &QTimer::timeout, this, &Session::refreshAnnouncements);
		refreshTimer->setInterval(1000 * 60 * 5);
		refreshTimer->start(refreshTimer->interval());
	}

	return m_publicListingClient;
}

void Session::makeAnnouncement(const QUrl &url)
{
	if(!url.isValid() || !m_config->isAllowedAnnouncementUrl(url)) {
		log(Log().about(Log::Level::Warn, Log::Topic::PubList).message("Announcement API URL not allowed: " + url.toString()));
		return;
	}

	// Don't announce twice at the same server
	for(const sessionlisting::Announcement &a : m_publicListings) {
		if(a.apiUrl == url)
			return;
	}

	const bool privateUserList = m_config->getConfigBool(config::PrivateUserList);

	const sessionlisting::Session s {
		QString(),
		0,
		aliasOrId(),
		protocolVersion(),
		title(),
		userCount(),
		(hasPassword() || privateUserList) ? QStringList() : userNames(),
		hasPassword(),
		isNsfm(),
		founder(),
		sessionStartTime()
	};

	publicListingClient()->announceSession(url, s);
}

void Session::unlistAnnouncement(const QString &url, bool terminate, bool removeOnly)
{
	QMutableListIterator<sessionlisting::Announcement> i = m_publicListings;
	bool changed = false;
	while(i.hasNext()) {
		const sessionlisting::Announcement &a = i.next();
		if(a.apiUrl == url || url == QStringLiteral("*")) {
			if(!removeOnly)
				publicListingClient()->unlistSession(a);

			if(terminate)
				m_history->removeAnnouncement(a.apiUrl.toString());

			i.remove();
			changed = true;
		}
	}

	if(changed)
		sendUpdatedAnnouncementList();
}

void Session::refreshAnnouncements()
{
	const bool privateUserList = m_config->getConfigBool(config::PrivateUserList);

	for(const sessionlisting::Announcement &a : m_publicListings) {
		m_publicListingClient->refreshSession(a, {
			QString(),
			0,
			QString(),
			protocol::ProtocolVersion(),
			title(),
			userCount(),
			hasPassword() || privateUserList ? QStringList() : userNames(),
			hasPassword(),
			isNsfm(),
			founder(),
			sessionStartTime()
		});
	}
}

void Session::sessionAnnounced(const sessionlisting::Announcement &announcement)
{
	// Make sure there are no double announcements
	for(const sessionlisting::Announcement &a : m_publicListings) {
		if(a.apiUrl == announcement.apiUrl) {
			log(Log().about(Log::Level::Warn, Log::Topic::PubList).message("Double announcement at: " + announcement.apiUrl.toString()));
			return;
		}
	}

	m_history->addAnnouncement(announcement.apiUrl.toString());
	m_publicListings << announcement;
	sendUpdatedAnnouncementList();
}

void Session::sessionAnnouncementError(const QString &apiUrl)
{
	// Remove listing on error
	unlistAnnouncement(apiUrl, true, true);
}

void Session::historyCacheCleanup()
{
	int minIdx = m_history->lastIndex();
	for(const Client *c : m_clients) {
		minIdx = qMin(c->historyPosition(), minIdx);
	}
	m_history->cleanupBatches(minIdx);
}

QJsonObject Session::getDescription(bool full) const
{
	QJsonObject o;
	// The basic description contains just the information
	// needed for the login session listing
	o["id"] = idString();
	o["alias"] = idAlias();
	o["protocol"] = protocolVersion().asString();
	o["userCount"] = userCount();
	o["maxUserCount"] = maxUsers();
	o["founder"] = founder();
	o["title"] = title();
	o["hasPassword"] = hasPassword();
	o["closed"] = isClosed();
	if(m_config->getConfigBool(config::EnablePersistence))
		o["persistent"] = isPersistent();
	o["nsfm"] = isNsfm();
	o["startTime"] = sessionStartTime().toString();
	o["size"] = int(m_history->sizeInBytes());

	if(full) {
		// Full descriptions includes detailed info for server admins.
		o["maxSize"] = int(m_history->sizeLimit());

		QJsonArray users;
		for(const Client *user : m_clients) {
			users << user->description(false);
		}
		o["users"] = users;
	}

	return o;
}

JsonApiResult Session::callJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	if(!path.isEmpty()) {
		QString head;
		QStringList tail;
		std::tie(head, tail) = popApiPath(path);

		int userId = head.toInt();
		if(userId>0) {
			Client *c = getClientById(userId);
			if(c)
				c->callJsonApi(method, tail, request);
		}

		return JsonApiNotFound();
	}

	if(method == JsonApiMethod::Update) {
		setSessionConfig(request);

		if(request.contains("message"))
			messageAll(request["message"].toString(), false);
		if(request.contains("alert"))
			messageAll(request["alert"].toString(), true);

	} else if(method == JsonApiMethod::Delete) {
		killSession();
		QJsonObject o;
		o["status"] = "ok";
		return JsonApiResult{JsonApiResult::Ok, QJsonDocument(o)};
	}

	return JsonApiResult{JsonApiResult::Ok, QJsonDocument(getDescription(true))};
}

void Session::log(Log entry)
{
	entry.session(id());
	m_config->logger()->logMessage(entry);
}

}
