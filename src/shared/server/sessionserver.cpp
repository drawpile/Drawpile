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
#include "inmemoryhistory.h"
#include "filedhistory.h"

#include "../util/logger.h"
#include "../util/announcementapi.h"

#include <QTimer>
#include <QJsonArray>
#include <QJsonDocument>

namespace server {

SessionServer::SessionServer(ServerConfig *config, QObject *parent)
	: QObject(parent),
	m_config(config),
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
		logger::warning() << dir.absolutePath() << "is not readable";
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
			logger::info() << session << "loaded.";
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

Session *SessionServer::createSession(const QUuid &id, const QString &idAlias, const protocol::ProtocolVersion &protocolVersion, const QString &founder)
{
	Q_ASSERT(!id.isNull());
	Q_ASSERT(!getSessionById(id.toString()));

	SessionHistory *h;
	if(m_useFiledSessions) {
		FiledHistory *fh = FiledHistory::startNew(m_sessiondir, id, idAlias, protocolVersion, founder);
		fh->setArchive(m_config->getConfigBool(config::ArchiveMode));
		h = fh;
	} else {
		h = new InMemoryHistory(id, idAlias, protocolVersion, founder);
	}
	Session *session = new Session(h, m_config, this);

	initSession(session);

	logger::debug() << session << "Session created by" << founder;

	return session;
}

void SessionServer::initSession(Session *session)
{
	m_sessions.append(session);

	connect(session, &Session::userConnected, this, &SessionServer::moveFromLobby);
	connect(session, &Session::userDisconnected, this, &SessionServer::userDisconnectedEvent);
	connect(session, &Session::sessionAttributeChanged, [this](Session *ses) { emit sessionChanged(ses->getDescription()); });
	connect(session, &Session::destroyed, [this, session]() {
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
	logger::debug() << client << "moved from lobby to" << session;
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
		logger::debug() << "non-logged in client from" << client->peerAddress() << "removed";
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
		logger::debug() << session << "Last user left";

		// A non-persistent session is deleted when the last user leaves
		// A persistent session can also be deleted if it doesn't contain a snapshot point.
		if(!session->isPersistent()) {
			logger::info() << session << "Closing non-persistent session";
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
					logger::info() << s << "Idle session expired. Uptime was" << s->uptime();
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
