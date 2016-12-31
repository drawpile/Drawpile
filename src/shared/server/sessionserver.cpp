/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include "../util/logger.h"
#include "../util/announcementapi.h"

#include <QTimer>
#include <QJsonArray>

namespace server {

SessionServer::SessionServer(ServerConfig *config, QObject *parent)
	: QObject(parent),
	m_config(config),
	m_mustSecure(false)
{
	QTimer *cleanupTimer = new QTimer(this);
	connect(cleanupTimer, &QTimer::timeout, this, &SessionServer::cleanupSessions);
	cleanupTimer->setInterval(15 * 1000);
	cleanupTimer->start(cleanupTimer->interval());

	m_publicListingApi = new sessionlisting::AnnouncementApi(this);

	connect(m_publicListingApi, &sessionlisting::AnnouncementApi::sessionAnnounced, this, &SessionServer::sessionAnnounced);
	connect(m_publicListingApi, &sessionlisting::AnnouncementApi::messageReceived, [this](const QString &msg) {
		wall("Session announced: " + msg);
	});

	QTimer *announcementRefreshTimer = new QTimer(this);
	connect(announcementRefreshTimer, &QTimer::timeout, this, &SessionServer::refreshSessionAnnouncements);
	announcementRefreshTimer->setInterval(1000 * 60 * 5);
	announcementRefreshTimer->start(announcementRefreshTimer->interval());

#ifndef NDEBUG
	_randomlag = 0;
#endif
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

	Session *session = new Session(id, idAlias, protocolVersion, founder, m_config, this);

	initSession(session);

	logger::debug() << session << "Session created by" << founder;

	return session;
}

void SessionServer::initSession(Session *session)
{
	connect(session, &Session::userConnected, this, &SessionServer::moveFromLobby);
	connect(session, &Session::userDisconnected, this, &SessionServer::userDisconnectedEvent);
	connect(session, &Session::sessionAttributeChanged, [this](Session *ses) { emit sessionChanged(ses->getDescription()); });

	connect(session, &Session::requestAnnouncement, this, &SessionServer::announceSession);
	connect(session, &Session::requestUnlisting, this, &SessionServer::unlistSession);

	m_sessions.append(session);

	emit sessionCreated(session);
	emit sessionChanged(session->getDescription());
}

/**
 * @brief Delete a session
 *
 * If the session is hibernatable, it will be stored before it is deleted.
 *
 * @param session
 */
void SessionServer::destroySession(Session *session)
{
	Q_ASSERT(session);
	Q_ASSERT(m_sessions.contains(session));

	session->unlistAnnouncement();

	logger::debug() << session << "Deleting session. User count is" << session->userCount();
	m_sessions.removeOne(session);

	session->stopRecording();

	session->deleteLater(); // destroySession call might be triggered by a signal emitted from the session
	emit sessionEnded(session->idString());
}

QJsonObject SessionServer::getSessionDescriptionById(const QString &id, bool full) const
{
	const Session *s = getSessionById(id);
	if(s)
		return s->getDescription(full);

	return QJsonObject();
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

bool SessionServer::killSession(const QString &id)
{
	Session *s = getSessionById(id);
	if(s) {
		logger::info() << "Killing session" << id;
		s->killSession();
		if(s->userCount()==0)
			destroySession(s);
		return true;
	} else {
		logger::info() << "Cannot kill non-existent session" << id;
		return false;
	}
}

void SessionServer::stopAll()
{
	for(Client *c : m_lobby)
		c->disconnectShutdown();

	auto sessions = m_sessions;
	sessions.detach();
	for(Session *s : sessions) {
		s->stopRecording();
		s->kickAllUsers();

		if(s->userCount()==0)
			destroySession(s);
	}
}

bool SessionServer::wall(const QString &message, const QString &sessionId)
{
	bool found = false;
	for(Session *s : m_sessions) {
		if(sessionId.isNull() || s->idString() == sessionId || s->idAlias() == sessionId) {
			s->wall(message);
			found = true;
		}
	}
	return found;
}

void SessionServer::addClient(Client *client)
{
	client->setParent(this);
	client->setConnectionTimeout(m_config->getConfigTime(config::ClientTimeout) * 1000);

#ifndef NDEBUG
	client->setRandomLag(_randomlag);
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

	emit userLoggedIn();
	emit sessionChanged(session->getDescription());
}

/**
 * @brief Handle client disconnect while the client was not yet logged in
 * @param client
 */
void SessionServer::lobbyDisconnectedEvent(Client *client)
{
	logger::debug() << "non-logged in client from" << client->peerAddress() << "removed";
	Q_ASSERT(m_lobby.contains(client));
	m_lobby.removeOne(client);

	client->deleteLater();
	emit userDisconnected();
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
			logger::info() << session << "History size was" << session->mainstream().lengthInBytes() << "bytes";
			delSession = true;
		}
	}

	if(delSession)
		destroySession(session);
	else
		emit sessionChanged(session->getDescription());

	emit userDisconnected();
}

void SessionServer::cleanupSessions()
{
	const int expirationTime = m_config->getConfigTime(config::IdleTimeLimit);

	if(expirationTime>0) {
		QDateTime now = QDateTime::currentDateTime();

		QList<Session*> expirelist;

		for(Session *s : m_sessions) {
			if(s->userCount()==0) {
				if(s->lastEventTime().msecsTo(now) > expirationTime) {
					expirelist << s;
				}
			}
		}

		for(Session *s : expirelist) {
			logger::info() << s << "Idle session expired. Uptime was" << s->uptime();

			destroySession(s);
		}
	}
}

void SessionServer::refreshSessionAnnouncements()
{
	const bool privateUserList = m_config->getConfigBool(config::PrivateUserList);

	for(Session *s : m_sessions) {
		if(s->publicListing().listingId>0) {
			m_publicListingApi->refreshSession(s->publicListing(), {
				QString(),
				0,
				QString(),
				protocol::ProtocolVersion(),
				s->title(),
				s->userCount(),
				s->hasPassword() || privateUserList ? QStringList() : s->userNames(),
				s->hasPassword(),
				s->isNsfm(),
				s->founder(),
				s->sessionStartTime()
			});
		}
	}
}

void SessionServer::announceSession(const QUrl &url, const sessionlisting::Session &session)
{
	if(m_config->isAllowedAnnouncementUrl(url)) {
		m_publicListingApi->announceSession(url, session);
	} else {
		logger::warning() << "Announcement API URL not allowed:" << url.toString();
	}
}

void SessionServer::unlistSession(const sessionlisting::Announcement &listing)
{
	m_publicListingApi->unlistSession(listing);
}

void SessionServer::sessionAnnounced(const sessionlisting::Announcement &listing)
{
	Session *s = getSessionById(listing.id);
	if(!s) {
		logger::warning() << "Announced non-existent session" << listing.id;
		return;
	}

	s->setPublicListing(listing);
}

}
