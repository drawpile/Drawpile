/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2015 Calle Laakkonen

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
#include "../net/control.h"
#include "../net/meta.h"
#include "../record/writer.h"
#include "../util/passwordhash.h"
#include "../util/filename.h"

#include "config.h"

#include <QTimer>

namespace server {

using protocol::MessagePtr;

Session::Session(const QUuid &id, const QString &alias, const protocol::ProtocolVersion &protocolVersion, const QString &founder, ServerConfig *config, QObject *parent)
	: QObject(parent),
	m_config(config),
	m_state(Initialization),
	m_initUser(-1),
	m_recorder(0),
	m_lastUserId(0),
	m_startTime(QDateTime::currentDateTime()), m_lastEventTime(QDateTime::currentDateTime()),
	m_id(id), m_idAlias(alias), m_protocolVersion(protocolVersion), m_maxusers(254),
	m_founder(founder),
	m_closed(false),
	m_persistent(false), m_preserveChat(false), m_nsfm(false),
	m_historytLimitWarningSent(false)
{
	// Cache history limit, since this value is checked very often
	m_historylimit = config->getConfigSize(config::SessionSizeLimit);
	m_historyLimitWarning = m_historylimit * 0.7;
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

		if(m_state==Reset) {
			protocol::ServerReply resetcmd;
			resetcmd.type = protocol::ServerReply::RESET;
			resetcmd.reply["state"] = "reset";
			resetcmd.message = "Session reset!";
			MessagePtr resetmsg(new protocol::Command(0, resetcmd));

			// Inform everyone of the reset
			for(Client *c : m_clients)
				c->sendDirectMessage(resetmsg);

			// Update current state
			QList<uint8_t> owners;
			for(Client *c : m_clients) {
				addToCommandStream(c->joinMessage());
				if(c->isOperator())
					owners << c->id();
			}
			addToCommandStream(protocol::MessagePtr(new protocol::SessionOwner(0, owners)));
			sendUpdatedSessionProperties();

			// Send reset snapshot
			qDebug("Reset stream size %d", m_resetstream.size());
			for(const MessagePtr &m : m_resetstream)
				addToCommandStream(m);

			m_resetstream.clear();
		}

		for(Client *c : m_clients)
			c->enqueueHeldCommands();

	} else if(newstate==Reset) {
		if(m_state!=Running)
			qFatal("Illegal state change to Reset from %d", m_state);

		Q_ASSERT(m_resetstream.isEmpty());

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
	connect(this, &Session::newCommandsAvailable, user, &Client::sendAvailableCommands);

	addToCommandStream(user->joinMessage());

	if(host) {
		logger::info() << "join user. State is" << m_state;
		Q_ASSERT(m_state == Initialization);

		m_initUser = user->id();
	}

	const QString welcomeMessage = m_config->getConfigString(config::WelcomeMessage);
	if(!welcomeMessage.isEmpty()) {
		user->sendSystemChat(welcomeMessage);
	}

	ensureOperatorExists();

	// Let new users know about the size limit too
	if(m_mainstream.lengthInBytes() > m_historyLimitWarning) {
		protocol::ServerReply warning;
		warning.type = protocol::ServerReply::SIZELIMITWARNING;
		warning.reply["size"] = int(m_mainstream.lengthInBytes());
		warning.reply["maxSize"] = int(m_historyLimitWarning);
		user->sendDirectMessage(protocol::MessagePtr(new protocol::Command(0, warning)));
		logger::info() << "notified new user about the limit";
	}

	logger::info() << user << "Joined session";
	emit userConnected(this, user);
}

void Session::removeUser(Client *user)
{
	Q_ASSERT(m_clients.contains(user));

	if(user->id() == m_initUser && m_state == Reset) {
		// Whoops, the resetter left before the job was done!
		// We simply cancel the reset in that case and go on
		m_initUser = 0;
		m_resetstream.clear();
		switchState(Running);
	}

	m_clients.removeOne(user);

	addToCommandStream(MessagePtr(new protocol::UserLeave(user->id())));

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

	if(conf.contains("persistent")) {
		m_persistent = conf["persistent"].toBool() && m_config->getConfigBool(config::EnablePersistence);
		changed = true;
	}

	if(conf.contains("title")) {
		QString newTitle = conf["title"].toString().mid(0, 100);
		m_title = newTitle;
		changed = true;
	}

	if(conf.contains("maxUserCount")) {
		const int newUserCount = conf["maxUserCount"].toInt();
		if(newUserCount>0 && newUserCount<255) {
			m_maxusers = newUserCount;
			changed = true;
		}
	}

	if(conf.contains("password")) {
		QString pw = conf["password"].toString();
		if(pw.isEmpty())
			m_passwordhash = QByteArray();
		else
			m_passwordhash = passwordhash::hash(pw);
		changed = true;
	}

	// Note: this bit is only relayed by the server: it informs
	// the client whether to send preserved/recorded chat messages
	// by default.
	if(conf.contains("preserveChat")) {
		m_preserveChat = conf["preserveChat"].toBool();
		changed = true;
	}

	// "Not Safe For Minors" tag can only be unset by resetting the session
	if(conf["nsfm"].toBool()) {
		m_nsfm = true;
		changed = true;
	}

	if(changed)
		sendUpdatedSessionProperties();
}

bool Session::checkPassword(const QString &password) const
{
	return passwordhash::check(password, m_passwordhash);
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
	conf["closed"] = isClosed();
	conf["persistent"] = isPersistent();
	conf["title"] = title();
	conf["maxUserCount"] = m_maxusers;
	conf["preserveChat"] = m_preserveChat;
	conf["nsfm"] = m_nsfm;
	props.reply["config"] = conf;

	addToCommandStream(protocol::MessagePtr(new protocol::Command(0, props)));
	emit sessionAttributeChanged(this);
}

void Session::addToCommandStream(protocol::MessagePtr msg)
{
	if(m_state == Shutdown)
		return;

	if(m_historylimit>0 && m_mainstream.lengthInBytes() + msg->length() > m_historylimit) {
		wall("History size limit reached! Session must be reset to continue drawing");
		return;
	}

	m_mainstream.append(msg);
	if(m_recorder)
		m_recorder->recordMessage(msg);
	m_lastEventTime = QDateTime::currentDateTime();
	emit newCommandsAvailable();

	if(m_historylimit>0) {
		// Send a warning if approaching size limit.
		// The clients should update their internal size limits
		// and reset the session when necessary.
		const uint hlen = m_mainstream.lengthInBytes();
		if(hlen > m_historyLimitWarning && !m_historytLimitWarningSent) {
			logger::debug() << this << "history limit warning threshold reached.";

			protocol::ServerReply warning;
			warning.type = protocol::ServerReply::SIZELIMITWARNING;
			warning.reply["size"] = int(hlen);
			warning.reply["maxSize"] = int(m_historyLimitWarning);

			protocol::MessagePtr msg(new protocol::Command(0, warning));
			for(Client *c : m_clients) {
				c->sendDirectMessage(msg);
				m_historytLimitWarningSent = true;
			}
		}
	}
}

void Session::addToInitStream(protocol::MessagePtr msg)
{
	Q_ASSERT(m_state == Initialization || m_state == Reset || m_state == Shutdown);

	if(m_state == Initialization) {
		addToCommandStream(msg);
	} else if(m_state == Reset) {
		m_resetstream.append(msg);
	}
}

void Session::handleInitComplete(int ctxId)
{
	if(ctxId != m_initUser) {
		logger::warning() << this << "User" << ctxId << "sent init-complete, but init user is" << m_initUser;
		return;
	}

	if(m_state == Reset) {
		m_mainstream.resetTo(m_mainstream.end());
	}

	logger::debug() << this << "init-complete by user" << ctxId;
	switchState(Running);
}

void Session::resetSession(int resetter)
{
	Q_ASSERT(m_state == Running);

	m_initUser = resetter;
	switchState(Reset);

	protocol::ServerReply resetRequest;
	resetRequest.type = protocol::ServerReply::RESET;
	resetRequest.reply["state"] = "init";
	resetRequest.message = "Prepared to receive session data";

	m_historytLimitWarningSent = false;

	getClientById(resetter)->sendDirectMessage(protocol::MessagePtr(new protocol::Command(0, resetRequest)));
}

void Session::killSession()
{
	switchState(Shutdown);
	unlistAnnouncement();
	stopRecording();

	for(Client *c : m_clients)
		c->disconnectShutdown();
	m_clients.clear();

	this->deleteLater();
}

void Session::wall(const QString &message)
{
	for(Client *c : m_clients) {
		c->sendSystemChat(message);
	}
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
		addToCommandStream(protocol::MessagePtr(new protocol::SessionOwner(0, ids)));
	}
}

void Session::startRecording(const QList<protocol::MessagePtr> &snapshot)
{
	Q_ASSERT(m_recorder==0);

	// Start recording
	QString filename = utils::makeFilenameUnique(m_recordingFile, ".dprec");
	logger::info() << this << "Starting session recording" << filename;

	m_recorder = new recording::Writer(filename, this);
	if(!m_recorder->open()) {
		logger::error() << this << "Couldn't write session recording to" << filename << m_recorder->errorString();
		delete m_recorder;
		m_recorder = 0;
		return;
	}

	QJsonObject metadata;
	metadata["server-recording"] = true;
	metadata["version"] = m_protocolVersion.asString();

	m_recorder->writeHeader(metadata);

	// Record snapshot and what is in the main stream
	for(protocol::MessagePtr msg : snapshot) {
		m_recorder->recordMessage(msg);
	}

	for(int i=m_mainstream.offset();i<m_mainstream.end();++i)
		m_recorder->recordMessage(m_mainstream.at(i));
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
	qint64 up = (QDateTime::currentMSecsSinceEpoch() - m_startTime.toMSecsSinceEpoch()) / 1000;

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
	o["persistent"] = isPersistent();
	o["nsfm"] = isNsfm();
	o["startTime"] = sessionStartTime().toString();
	o["size"] = int(m_mainstream.lengthInBytes());

	if(full) {
		// Full descriptions includes detailed info for server admins.
		o["maxSize"] = int(m_historylimit);

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
