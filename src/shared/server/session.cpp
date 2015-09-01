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
#include "../net/control.h"
#include "../net/meta.h"
#include "../record/writer.h"
#include "../util/passwordhash.h"
#include "../util/filename.h"

#include "config.h"

#include <QTimer>

namespace server {

using protocol::MessagePtr;

Session::Session(const SessionId &id, int minorVersion, const QString &founder, QObject *parent)
	: QObject(parent),
	m_state(Initialization),
	m_initUser(-1),
	m_recorder(0),
	m_lastUserId(0),
	m_startTime(QDateTime::currentDateTime()), m_lastEventTime(QDateTime::currentDateTime()),
	m_id(id), m_minorVersion(minorVersion), m_maxusers(255), m_historylimit(0),
	m_founder(founder),
	m_closed(false),
	m_allowPersistent(false), m_persistent(false), m_hibernatable(false), m_preserveChat(false)
{
}

QString Session::toLogString() const {
	return QStringLiteral("Session %1:").arg(id());
}

void Session::switchState(State newstate)
{
	if(newstate==Initialization) {
		qFatal("Illegal state change to Initialization from %d", m_state);

	} else if(newstate==Running) {
		if(m_state!=Initialization && m_state!=Reset)
			qFatal("Illegal state change to Running from %d", m_state);

		m_resetstream.clear();
		m_initUser = -1;

		for(Client *c : m_clients)
			c->enqueueHeldCommands();

	} else if(newstate==Reset) {
		if(m_state!=Running)
			qFatal("Illegal state change to Reset from %d", m_state);

		Q_ASSERT(m_resetstream.isEmpty());

	} else if(newstate==Shutdown) {
		unlistAnnouncement();
		setClosed(true);
		stopRecording();

		for(Client *c : m_clients)
			c->disconnectKick(QString());
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

	addToCommandStream(MessagePtr(new protocol::UserJoin(
		user->id(),
		(user->isAuthenticated() ? protocol::UserJoin::FLAG_AUTH : 0) | (user->isModerator() ? protocol::UserJoin::FLAG_MOD : 0),
		user->username()
	)));

	if(host) {
		Q_ASSERT(m_state == Initialization);

		m_initUser = user->id();
	}

	if(!welcomeMessage().isEmpty()) {
		user->sendSystemChat(welcomeMessage());
	}

	ensureOperatorExists();

	m_lastEventTime = QDateTime::currentDateTime();

	logger::info() << user << "Joined session";
	emit userConnected(this, user);
}

void Session::removeUser(Client *user)
{
	Q_ASSERT(m_clients.contains(user));

	if(user->id() == m_initUser && m_state == Reset) {
		qDebug("TODO abort reset");
		switchState(Running);
	}

	m_clients.removeOne(user);

	addToCommandStream(MessagePtr(new protocol::UserLeave(user->id())));

	ensureOperatorExists();

	// Reopen the session when the last user leaves
	if(m_clients.isEmpty()) {
		setClosed(false);
	}

	user->deleteLater();

	m_lastEventTime = QDateTime::currentDateTime();

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

void Session::setPassword(const QString &password)
{
	if(password.isEmpty())
		setPasswordHash(QByteArray());
	else
		setPasswordHash(passwordhash::hash(password));
}

void Session::setPasswordHash(const QByteArray &passwordhash)
{
	m_passwordhash = passwordhash;
	sendUpdatedSessionProperties();
}

void Session::setSessionConfig(const QJsonObject &conf)
{
	if(conf.contains("closed"))
		m_closed = conf["closed"].toBool();

	if(conf.contains("persistent"))
		m_persistent = m_allowPersistent && conf["persistent"].toBool();

	if(conf.contains("title"))
		m_title = conf["title"].toString();

	if(conf.contains("max-users"))
		m_maxusers = qBound(1, conf["max-users"].toInt(), 254);

	if(conf.contains("password")) {
		QString pw = conf["password"].toString();
		if(pw.isEmpty())
			m_passwordhash = QByteArray();
		else
			m_passwordhash = passwordhash::hash(pw);
	}

	// Note: this bit is only relayed by the server: it informs
	// the client whether to send preserved/recorded chat messages
	// by default.
	if(conf.contains("preserve-chat"))
		m_preserveChat = conf["preserve-chat"].toBool();

	sendUpdatedSessionProperties();
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
	conf["max-users"] = m_maxusers;
	conf["preserve-chat"] = m_preserveChat;
	props.reply["config"] = conf;

	addToCommandStream(protocol::MessagePtr(new protocol::Command(0, props)));
	emit sessionAttributeChanged(this);
}

void Session::addToCommandStream(protocol::MessagePtr msg)
{
	m_mainstream.append(msg);
	if(m_recorder)
		m_recorder->recordMessage(msg);
	emit newCommandsAvailable();

	if(m_historylimit>0 && m_mainstream.lengthInBytes() > m_historylimit) {
		wall("Session size limit reached!");
		killSession();
	}
}

void Session::addToInitStream(protocol::MessagePtr msg)
{
	Q_ASSERT(m_state == Initialization || m_state == Reset);

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
		// TODO replace history with reset
	}

	logger::debug() << this << "init-complete by user" << ctxId;
	switchState(Running);
}

/**
 * @brief Kick all users off the server
 */
void Session::kickAllUsers()
{
	for(Client *c : m_clients)
		c->disconnectShutdown();
}

void Session::killSession()
{
	setHibernatable(false);
	m_persistent = false;
	switchState(Shutdown);
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

	m_recorder->writeHeader();

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
		m_recorder = 0;
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

void Session::makeAnnouncement(const QUrl &url)
{
	sessionlisting::Session s {
		QString(),
		0,
		id(),
		QStringLiteral("%1.%2").arg(DRAWPILE_PROTO_MAJOR_VERSION).arg(minorProtocolVersion()),
		title(),
		userCount(),
		!passwordHash().isEmpty(),
		false, // TODO: explicit NSFM tag
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

}
