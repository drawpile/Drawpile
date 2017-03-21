/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2017 Calle Laakkonen

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

#include "sessionserver.h"
#include "session.h"
#include "client.h"
#include "loginhandler.h"
#include "serverconfig.h"
#include "serverlog.h"
#include "inmemoryhistory.h"
#include "filedhistory.h"
#include "templateloader.h"

#include "../util/announcementapi.h"

#include <QTimer>
#include <QJsonArray>
#include <QJsonDocument>

namespace server {

SessionServer::SessionServer(ServerConfig *config, QObject *parent)
	: QObject(parent),
	m_config(config),
	m_tpls(nullptr),
	m_useFiledSessions(false),
	m_mustSecure(false)
{
	QTimer *cleanupTimer = new QTimer(this);
	connect(cleanupTimer, &QTimer::timeout, this, &SessionServer::cleanupSessions);
	cleanupTimer->setInterval(15 * 1000);
	cleanupTimer->start(cleanupTimer->interval());

#ifndef NDEBUG
	m_randomlag = 0;
#endif
}

void SessionServer::setSessionDir(const QDir &dir)
{
	if(dir.isReadable()) {
		m_sessiondir = dir;
		m_useFiledSessions = true;
		loadNewSessions();
	} else {
		qWarning("%s is not readable", qPrintable(dir.absolutePath()));
	}
}

void SessionServer::loadNewSessions()
{
	if(!m_useFiledSessions)
		return;

	auto sessionFiles = m_sessiondir.entryInfoList(QStringList() << "*.session", QDir::Files|QDir::Writable|QDir::Readable);
	for(const QFileInfo &f : sessionFiles) {
		if(getSessionById(f.baseName()))
			continue;

		FiledHistory *fh = FiledHistory::load(f.absoluteFilePath());
		if(fh) {
			fh->setArchive(m_config->getConfigBool(config::ArchiveMode));
			Session *session = new Session(fh, m_config, this);
			initSession(session);
			session->log(Log().about(Log::Level::Debug, Log::Topic::Status).message("Loaded from file."));
		}
	}
}

QJsonArray SessionServer::sessionDescriptions() const
{
	QJsonArray descs;

	for(const Session *s : m_sessions)
		descs.append(s->getDescription());

	return descs;
}

SessionHistory *SessionServer::initHistory(const QUuid &id, const QString alias, const protocol::ProtocolVersion &protocolVersion, const QString &founder)
{
	if(m_useFiledSessions) {
		FiledHistory *fh = FiledHistory::startNew(m_sessiondir, id, alias, protocolVersion, founder);
		fh->setArchive(m_config->getConfigBool(config::ArchiveMode));
		return fh;
	} else {
		return new InMemoryHistory(id, alias, protocolVersion, founder);
	}
}

Session *SessionServer::createSession(const QUuid &id, const QString &idAlias, const protocol::ProtocolVersion &protocolVersion, const QString &founder)
{
	Q_ASSERT(!id.isNull());
	Q_ASSERT(!getSessionById(id.toString()));

	Session *session = new Session(initHistory(id, idAlias, protocolVersion, founder), m_config, this);

	initSession(session);

	QString aka = idAlias.isEmpty() ? QString() : QStringLiteral(" (AKA %1)").arg(idAlias);

	session->log(Log()
		.about(Log::Level::Info, Log::Topic::Status)
		.message("Session" + aka + " created by " + founder));

	return session;
}

Session *SessionServer::createFromTemplate(const QString &idAlias)
{
	Q_ASSERT(templateLoader());

	QJsonObject desc = templateLoader()->templateDescription(idAlias);
	if(desc.isEmpty())
		return nullptr;

	SessionHistory *history = initHistory(
		QUuid::createUuid(),
		idAlias, 
		protocol::ProtocolVersion::fromString(desc["protocol"].toString()),
		desc["founder"].toString());

	if(!templateLoader()->init(history)) {
		delete history;
		return nullptr;
	}

	Session *session = new Session(history, m_config, this);
	initSession(session);
	session->log(Log()
		.about(Log::Level::Info, Log::Topic::Status)
		.message(QStringLiteral("Session instantiated from template %1").arg(idAlias)));

	return session;
}

void SessionServer::initSession(Session *session)
{
	m_sessions.append(session);

	connect(session, &Session::userConnected, this, &SessionServer::moveFromLobby);
	connect(session, &Session::userDisconnected, this, &SessionServer::userDisconnectedEvent);
	connect(session, &Session::sessionAttributeChanged, this, [this](Session *ses) { emit sessionChanged(ses->getDescription()); });
	connect(session, &Session::destroyed, this, [this, session]() {
		m_sessions.removeOne(session);
		emit sessionEnded(session->idString());
	});

	emit sessionCreated(session);
	emit sessionChanged(session->getDescription());
}

Session *SessionServer::getSessionById(const QString &id) const
{
	const QUuid uuid(id);
	for(Session *s : m_sessions) {
		if(uuid.isNull()) {
			if(s->idAlias() == id)
				return s;
		} else {
			if(s->id() == uuid)
				return s;
		}
	}

	return nullptr;
}

bool SessionServer::isIdInUse(const QString &id) const
{
	// Check live sessions
	const QUuid uuid(id);
	for(const Session *s : m_sessions) {
		if(uuid.isNull()) {
			if(s->idAlias() == id)
				return true;
		} else {
			if(s->id() == uuid)
				return true;
		}
	}

	// Check templates
	if(templateLoader() && templateLoader()->exists(id))
		return true;

	return false;
}

int SessionServer::totalUsers() const
{
	int count = m_lobby.size();
	for(const Session * s : m_sessions)
		count += s->userCount();
	return count;
}

void SessionServer::stopAll()
{
	for(Client *c : m_lobby)
		c->disconnectShutdown();

	auto sessions = m_sessions;
	sessions.detach();
	for(Session *s : sessions) {
		s->killSession(false);
	}
}

void SessionServer::messageAll(const QString &message, bool alert)
{
	for(Session *s : m_sessions) {
		s->messageAll(message, alert);
	}
}

void SessionServer::addClient(Client *client)
{
	client->setParent(this);
	client->setConnectionTimeout(m_config->getConfigTime(config::ClientTimeout) * 1000);

#ifndef NDEBUG
	client->setRandomLag(m_randomlag);
#endif

	m_lobby.append(client);

	connect(client, &Client::loggedOff, this, &SessionServer::lobbyDisconnectedEvent);

	(new LoginHandler(client, this))->startLoginProcess();
}

/**
 * @brief Handle the move of a client from the lobby to a session
 * @param session
 * @param client
 */
void SessionServer::moveFromLobby(Session *session, Client *client)
{
	Q_ASSERT(m_lobby.contains(client));
	m_lobby.removeOne(client);

	// the session handles disconnect events from now on
	disconnect(client, &Client::loggedOff, this, &SessionServer::lobbyDisconnectedEvent);

	emit userLoggedIn(totalUsers());
	emit sessionChanged(session->getDescription());
}

/**
 * @brief Handle client disconnect while the client was not yet logged in
 * @param client
 */
void SessionServer::lobbyDisconnectedEvent(Client *client)
{
	if(m_lobby.removeOne(client)) {
		client->log(Log().about(Log::Level::Info, Log::Topic::Leave).message("Non-logged in client removed."));
		disconnect(client, &Client::loggedOff, this, &SessionServer::lobbyDisconnectedEvent);
		emit userDisconnected(totalUsers());
	}
}

/**
 * @brief Handle client disconnect from a session
 *
 * The session takes care of the client itself. Here, we clean up after the session
 * in case it needs to be closed.
 * @param session
 */
void SessionServer::userDisconnectedEvent(Session *session)
{
	bool delSession = false;
	if(session->userCount()==0) {
		session->log(Log().about(Log::Level::Info, Log::Topic::Status).message("Last user left."));

		// A non-persistent session is deleted when the last user leaves
		// A persistent session can also be deleted if it doesn't contain a snapshot point.
		if(!session->isPersistent()) {
			session->log(Log().about(Log::Level::Info, Log::Topic::Status).message("Closing non-persistent session."));
			delSession = true;
		}
	}

	if(delSession)
		session->killSession();
	else
		emit sessionChanged(session->getDescription());

	emit userDisconnected(totalUsers());
}

void SessionServer::cleanupSessions()
{
	const int expirationTime = m_config->getConfigTime(config::IdleTimeLimit);

	if(expirationTime>0) {
		QDateTime now = QDateTime::currentDateTime();

		for(Session *s : m_sessions) {
			if(s->userCount()==0) {
				if(s->lastEventTime().msecsTo(now) > expirationTime) {
					s->log(Log().about(Log::Level::Info, Log::Topic::Status).message("Idle session expired."));
					s->killSession();
				}
			}
		}
	}
}

JsonApiResult SessionServer::callSessionJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	QString head;
	QStringList tail;
	std::tie(head, tail) = popApiPath(path);

	if(!head.isEmpty()) {
		Session *s = getSessionById(head);
		if(s)
			return s->callJsonApi(method, tail, request);
		else
			return JsonApiNotFound();
	}

	if(method == JsonApiMethod::Get) {
		return {JsonApiResult::Ok, QJsonDocument(sessionDescriptions())};

	} else if(method == JsonApiMethod::Update) {
		const QString msg = request["message"].toString();
		if(!msg.isEmpty())
			messageAll(msg, false);

		const QString alert = request["alert"].toString();
		if(!alert.isEmpty())
			messageAll(alert, true);

		return {JsonApiResult::Ok, QJsonDocument(QJsonObject())};


	} else {
		return JsonApiBadMethod();
	}
}

JsonApiResult SessionServer::callUserJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	Q_UNUSED(request);
	if(path.size()!=0)
		return JsonApiNotFound();

	if(method == JsonApiMethod::Get) {
		QJsonArray userlist;
		for(const Client *c : m_lobby)
			userlist << c->description();

		for(const Session *s : m_sessions) {
			for(const Client *c : s->clients())
				userlist << c->description();
		}

		return {JsonApiResult::Ok, QJsonDocument(userlist)};

	} else {
		return JsonApiBadMethod();
	}
}

}
