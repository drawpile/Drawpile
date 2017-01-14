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
	m_lastUserId(0),
	m_lastEventTime(QDateTime::currentDateTime()),
	m_closed(false),
	m_historyLimitWarningSent(false)
{
	m_history->setParent(this);
	m_history->setSizeLimit(config->getConfigSize(config::SessionSizeLimit));
	m_historyLimitWarning = m_history->sizeLimit() * 0.7;

	if(history->sizeInBytes()>0)
		m_state = Running;
}

QString Session::toLogString() const {
	if(idAlias().isEmpty())
		return QStringLiteral("Session %1:").arg(id().toString());
	else
		return QStringLiteral("Session %1|%2:").arg(id().toString(), idAlias());
}

void Session::switchState(State newstate)
{
	logger::debug() << this << "switch state to" << newstate << "from" << m_state;
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
	int loops=0;
	while(++loops<255) {
		++m_lastUserId;
		if(m_lastUserId>254)
			m_lastUserId=1;

		bool isFree = true;
		for(const Client *c : m_clients) {
			if(c->id() == m_lastUserId) {
				isFree = false;
				break;
			}
		}

		if(isFree) {
			user->setId(m_lastUserId);
			break;
		}

	}
	// shouldn't happen: we don't let users in if the session is full
	Q_ASSERT(user->id() != 0);
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
	}

	addToHistory(user->joinMessage());

	const QString welcomeMessage = m_config->getConfigString(config::WelcomeMessage);
	if(!welcomeMessage.isEmpty()) {
		user->sendSystemChat(welcomeMessage);
	}

	ensureOperatorExists();

	// Let new users know about the size limit too
	if(m_historyLimitWarning > 0 && m_history->sizeInBytes() > m_historyLimitWarning) {
		protocol::ServerReply warning;
		warning.type = protocol::ServerReply::SIZELIMITWARNING;
		warning.reply["size"] = int(m_history->sizeInBytes());
		warning.reply["maxSize"] = int(m_historyLimitWarning);
		user->sendDirectMessage(protocol::MessagePtr(new protocol::Command(0, warning)));
	}

	logger::info() << user << "Joined session";
	emit userConnected(this, user);
}

void Session::removeUser(Client *user)
{
	if(!m_clients.removeOne(user))
		return;

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

	ensureOperatorExists();

	// Reopen the session when the last user leaves
	if(m_clients.isEmpty()) {
		setClosed(false);
	}

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
	if(m_banlist.addBan(target->username(), target->peerAddress(), bannedBy)) {
		logger::info() << this << target->username() << "banned from session by" << bannedBy;
		// TODO structured logging
	}
}

void Session::removeBan(int entryId, const QString &removedBy)
{
	if(m_banlist.removeBan(entryId)) {
		logger::info() << this << "ban entry" << entryId << "removed by" << removedBy;
	}
}

void Session::setClosed(bool closed)
{
	if(m_closed != closed) {
		m_closed = closed;
		sendUpdatedSessionProperties();
	}
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
		flags.setFlag(SessionHistory::Persistent, conf["persistent"].toBool() && m_config->getConfigBool(config::EnablePersistence));
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

	// Note: this bit is only relayed by the server: it informs
	// the client whether to send preserved/recorded chat messages
	// by default.
	if(conf.contains("preserveChat")) {
		flags.setFlag(SessionHistory::PreserveChat, conf["preserveChat"].toBool());
	}

	if(conf.contains("nsfm")) {
		flags.setFlag(SessionHistory::Nsfm, conf["nsfm"].toBool());
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

QList<uint8_t> Session::updateOwnership(QList<uint8_t> ids)
{
	QList<uint8_t> truelist;
	for(Client *c : m_clients) {
		c->setOperator(ids.contains(c->id()) | c->isModerator());
		if(c->isOperator())
			truelist << c->id();
	}
	return truelist;
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
	props.reply["config"] = conf;

	addToHistory(protocol::MessagePtr(new protocol::Command(0, props)));
	emit sessionAttributeChanged(this);
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
		logger::debug() << this << "history limit warning threshold reached.";

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
	if(ctxId != m_initUser) {
		logger::warning() << this << "User" << ctxId << "sent init-complete, but init user is" << m_initUser;
		return;
	}

	logger::debug() << this << "init-complete by user" << ctxId;
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
	unlistAnnouncement();
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
		QList<uint8_t> ids;
		ids << m_clients.first()->id();
		updateOwnership(ids);
		addToHistory(protocol::MessagePtr(new protocol::SessionOwner(0, ids)));
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
	logger::info() << this << "Starting session recording" << filename;

	m_recorder = new recording::Writer(filename, this);
	if(!m_recorder->open()) {
		logger::error() << this << "Couldn't write session recording to" << filename << m_recorder->errorString();
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

void Session::makeAnnouncement(const QUrl &url)
{
	const bool privateUserList = m_config->getConfigBool(config::PrivateUserList);

	sessionlisting::Session s {
		QString(),
		0,
		idAlias().isEmpty() ? id().toString() : idAlias(),
		protocolVersion(),
		title(),
		userCount(),
		(hasPassword() || privateUserList) ? QStringList() : userNames(),
		hasPassword(),
		isNsfm(),
		founder(),
		sessionStartTime()
	};

	emit requestAnnouncement(url, s);
}

void Session::unlistAnnouncement()
{
	if(m_publicListing.listingId>0) {
		emit requestUnlisting(m_publicListing);
		m_publicListing.listingId=0;
	}
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
			QJsonObject u;
			u["id"] = user->id();
			u["name"] = user->username();
			u["ip"] = user->peerAddress().toString();
			u["auth"] = user->isAuthenticated();
			u["op"] = user->isOperator();
			u["mod"] = user->isModerator();
			u["tls"] = user->isSecure();
			users << u;
		}
		o["users"] = users;
	}

	return o;
}

JsonApiResult Session::callJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	// TODO user management
	if(!path.isEmpty())
		return JsonApiNotFound();

	if(method == JsonApiMethod::Update) {
		setSessionConfig(request);

	} else if(method == JsonApiMethod::Delete) {
		killSession();
		QJsonObject o;
		o["status"] = "ok";
		return JsonApiResult{JsonApiResult::Ok, QJsonDocument(o)};
	}

	return JsonApiResult{JsonApiResult::Ok, QJsonDocument(getDescription(true))};
}

}
