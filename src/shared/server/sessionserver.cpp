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
#include "sessiondesc.h"

#include "../util/logger.h"
#include "../util/announcementapi.h"

#include <QTimer>

namespace server {

SessionServer::SessionServer(QObject *parent)
	: QObject(parent),
	_store(nullptr),
	_identman(nullptr),
	_sessionLimit(1),
	_connectionTimeout(0),
	_historyLimit(0),
	_expirationTime(0),
	_allowPersistentSessions(false),
	_mustSecure(false)
{
	QTimer *cleanupTimer = new QTimer(this);
	connect(cleanupTimer, &QTimer::timeout, this, &SessionServer::cleanupSessions);
	cleanupTimer->setInterval(15 * 1000);
	cleanupTimer->start(cleanupTimer->interval());

	_publicListingApi = new sessionlisting::AnnouncementApi(this);

	connect(_publicListingApi, &sessionlisting::AnnouncementApi::sessionAnnounced, this, &SessionServer::sessionAnnounced);
	connect(_publicListingApi, &sessionlisting::AnnouncementApi::messageReceived, [this](const QString &msg) {
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

QList<SessionDescription> SessionServer::sessions() const
{
	QList<SessionDescription> descs;

	for(const Session *s : _sessions)
		descs.append(SessionDescription(*s));

	return descs;
}

Session *SessionServer::createSession(const SessionId &id, const QString &protocolVersion, const QString &founder)
{
	Q_ASSERT(!id.isEmpty());
	Q_ASSERT(getSessionDescriptionById(id.id()).id.isEmpty());

	Session *session = new Session(id, protocolVersion, founder, this);

	initSession(session);

	logger::debug() << session << "Session created by" << founder;

	return session;
}

void SessionServer::initSession(Session *session)
{
	session->setHistoryLimit(_historyLimit);
	session->setPersistenceAllowed(allowPersistentSessions());
	session->setWelcomeMessage(welcomeMessage());

	connect(session, &Session::userConnected, this, &SessionServer::moveFromLobby);
	connect(session, &Session::userDisconnected, this, &SessionServer::userDisconnectedEvent);
	connect(session, &Session::sessionAttributeChanged, [this](Session *ses) { emit sessionChanged(SessionDescription(*ses)); });

	connect(session, &Session::requestAnnouncement, this, &SessionServer::announceSession);
	connect(session, &Session::requestUnlisting, this, &SessionServer::unlistSession);

	_sessions.append(session);

	emit sessionCreated(session);
	emit sessionChanged(SessionDescription(*session));
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
	Q_ASSERT(_sessions.contains(session));

	session->unlistAnnouncement();

	logger::debug() << session << "Deleting session. User count is" << session->userCount();
	_sessions.removeOne(session);

	QString id = session->id();

	session->stopRecording();

	session->deleteLater(); // destroySession call might be triggered by a signal emitted from the session
	emit sessionEnded(id);
}

SessionDescription SessionServer::getSessionDescriptionById(const QString &id, bool getExtended, bool getUsers) const
{
	for(Session *s : _sessions) {
		if(s->id() == id)
			return SessionDescription(*s, getExtended, getUsers);
	}

	return SessionDescription();
}

Session *SessionServer::getSessionById(const QString &id)
{
	for(Session *s : _sessions) {
		if(s->id() == id)
			return s;
	}

	return nullptr;
}

int SessionServer::totalUsers() const
{
	int count = _lobby.size();
	for(const Session * s : _sessions)
		count += s->userCount();
	return count;
}

bool SessionServer::killSession(const QString &id)
{
	logger::info() << "Killing session" << id;

	for(Session *s : _sessions) {
		if(s->id() == id) {
			s->killSession();
			if(s->userCount()==0)
				destroySession(s);
			return true;
		}
	}

	// not found
	return false;
}

bool SessionServer::kickUser(const QString &sessionId, int userId)
{
	logger::info() << "Kicking user" << userId << "from" << sessionId;

	Session *session = getSessionById(sessionId);
	if(!session)
		return false;
	
	Client *c = session->getClientById(userId);
	if(!c)
		return false;
	
	c->disconnectKick(QString());
	return true;
}

void SessionServer::stopAll()
{
	for(Client *c : _lobby)
		c->disconnectShutdown();

	auto sessions = _sessions;
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
	for(Session *s : _sessions) {
		if(sessionId.isNull() || s->id() == sessionId) {
			s->wall(message);
			found = true;
		}
	}
	return found;
}

void SessionServer::addClient(Client *client)
{
	client->setParent(this);
	client->setConnectionTimeout(_connectionTimeout);

#ifndef NDEBUG
	client->setRandomLag(_randomlag);
#endif

	_lobby.append(client);

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
	Q_ASSERT(_lobby.contains(client));
	_lobby.removeOne(client);

	// the session handles disconnect events from now on
	disconnect(client, &Client::loggedOff, this, &SessionServer::lobbyDisconnectedEvent);

	emit userLoggedIn();
	emit sessionChanged(SessionDescription(*session));
}

/**
 * @brief Handle client disconnect while the client was not yet logged in
 * @param client
 */
void SessionServer::lobbyDisconnectedEvent(Client *client)
{
	logger::debug() << "non-logged in client from" << client->peerAddress() << "removed";
	Q_ASSERT(_lobby.contains(client));
	_lobby.removeOne(client);

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
		emit sessionChanged(SessionDescription(*session));

	emit userDisconnected();
}

void SessionServer::cleanupSessions()
{
	if(_allowPersistentSessions && _expirationTime>0) {
		QDateTime now = QDateTime::currentDateTime();

		QList<Session*> expirelist;

		for(Session *s : _sessions) {
			if(s->userCount()==0) {
				if(s->lastEventTime().msecsTo(now) > _expirationTime) {
					expirelist << s;
				}
			}
		}

		for(Session *s : expirelist) {
			logger::info() << s << "Vacant session expired. Uptime was" << s->uptime();

			destroySession(s);
		}
	}
}

void SessionServer::refreshSessionAnnouncements()
{
	for(Session *s : _sessions) {
		if(s->publicListing().listingId>0) {
			_publicListingApi->refreshSession(s->publicListing(), {
				QString(),
				0,
				QString(),
				QString(),
				s->title(),
				s->userCount(),
				!s->passwordHash().isEmpty(),
				false, // TODO: explicit NSFM tag
				s->founder(),
				s->sessionStartTime()
			});
		}
	}
}

void SessionServer::announceSession(const QUrl &url, const sessionlisting::Session &session)
{
	_publicListingApi->announceSession(url, session);
}

void SessionServer::unlistSession(const sessionlisting::Announcement &listing)
{
	_publicListingApi->unlistSession(listing);
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

void SessionServer::setWelcomeMessage(const QString &message)
{
	_welcomeMessage = message;
	for(Session *s : _sessions)
		s->setWelcomeMessage(message);
}

}
