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
#include "sessionstore.h"

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
	_mustSecure(false),
	m_privateUserList(false)
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

void SessionServer::setSessionStore(SessionStore *store)
{
	Q_ASSERT(store);
	_store = store;
	store->setParent(this);

	connect(store, SIGNAL(sessionAvailable(SessionDescription)), this, SIGNAL(sessionChanged(SessionDescription)));
}

QList<SessionDescription> SessionServer::sessions() const
{
	QList<SessionDescription> descs;

	for(const SessionState *s : _sessions)
		descs.append(SessionDescription(*s));

	if(_store)
		descs += _store->sessions();

	// Deduplicate session list. Duplicates happen when session templates are used.
	// The templates are always at the end of the list, so deduplicating
	// will remove them if an active or hibernated instance exists.
	QSet<QString> ids;
	QMutableListIterator<SessionDescription> it(descs);
	while(it.hasNext()) {
		const SessionDescription sd = it.next();
		if(ids.contains(sd.id.id()))
			it.remove();
		else
			ids.insert(sd.id.id());
	}

	return descs;
}

SessionState *SessionServer::createSession(const SessionId &id, int minorVersion, const QString &founder)
{
	Q_ASSERT(!id.isEmpty());
	Q_ASSERT(getSessionDescriptionById(id.id()).id.isEmpty());

	SessionState *session = new SessionState(id, minorVersion, founder, this);

	initSession(session);

	logger::debug() << session << "Session created by" << founder;

	return session;
}

void SessionServer::initSession(SessionState *session)
{
	session->setHistoryLimit(_historyLimit);
	session->setPersistenceAllowed(allowPersistentSessions());
	session->setWelcomeMessage(welcomeMessage());
	session->setPrivateUserList(m_privateUserList);

	connect(session, &SessionState::userConnected, this, &SessionServer::moveFromLobby);
	connect(session, &SessionState::userDisconnected, this, &SessionServer::userDisconnectedEvent);
	connect(session, &SessionState::sessionAttributeChanged, [this](SessionState *ses) { emit sessionChanged(SessionDescription(*ses)); });

	connect(session, &SessionState::requestAnnouncement, this, &SessionServer::announceSession);
	connect(session, &SessionState::requestUnlisting, this, &SessionServer::unlistSession);

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
void SessionServer::destroySession(SessionState *session)
{
	Q_ASSERT(_sessions.contains(session));

	session->unlistAnnouncement();

	logger::debug() << session << "Deleting session. User count is" << session->userCount();
	_sessions.removeOne(session);

	QString id = session->id();

	session->stopRecording();

	if(session->isHibernatable() && _store) {
		logger::info() << session << "Hibernating.";
		_store->storeSession(session);
	}

	session->deleteLater(); // destroySession call might be triggered by a signal emitted from the session
	emit sessionEnded(id);
}

SessionDescription SessionServer::getSessionDescriptionById(const QString &id, bool getExtended, bool getUsers) const
{
	for(SessionState *s : _sessions) {
		if(s->id() == id)
			return SessionDescription(*s, getExtended, getUsers);
	}

	if(_store)
		return _store->getSessionDescriptionById(id);

	return SessionDescription();
}

SessionState *SessionServer::getSessionById(const QString &id)
{
	for(SessionState *s : _sessions) {
		if(s->id() == id)
			return s;
	}

	if(_store) {
		SessionState *session = _store->takeSession(id);
		if(session) {
			session->setParent(this);
			initSession(session);
			logger::info() << session << "Restored from hibernation";
			return session;
		}
	}

	return nullptr;
}

int SessionServer::totalUsers() const
{
	int count = _lobby.size();
	for(const SessionState * s : _sessions)
		count += s->userCount();
	return count;
}

ServerStatus SessionServer::getServerStatus() const
{
	ServerStatus s;
	s.activeSessions = sessionCount();
	s.totalSessions = sessionCount() + (_store ? _store->sessions().size() : 0);
	s.totalUsers = totalUsers();
	s.maxActiveSessions = sessionLimit();
	s.needHostPassword = !_hostPassword.isEmpty();
	s.allowPersistentSessions = _allowPersistentSessions;
	s.secureMode = _mustSecure;
	s.hibernation = _store != nullptr;
	s.title = title();

	return s;
}

bool SessionServer::killSession(const QString &id)
{
	logger::info() << "Killing session" << id;

	for(SessionState *s : _sessions) {
		if(s->id() == id) {
			s->killSession();
			if(s->userCount()==0)
				destroySession(s);
			return true;
		}
	}

	// Not an active session? Maybe it's a stored session
	if(_store) {
		return _store->deleteSession(id);
	}

	// not found
	return false;
}

bool SessionServer::kickUser(const QString &sessionId, int userId)
{
	logger::info() << "Kicking user" << userId << "from" << sessionId;

	SessionState *session = getSessionById(sessionId);
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
	for(SessionState *s : sessions) {
		if(_store)
			s->setHibernatable(s->isPersistent() || _store->storeAllSessions());
		s->stopRecording();
		s->kickAllUsers();

		if(s->userCount()==0)
			destroySession(s);
	}
}

bool SessionServer::wall(const QString &message, const QString &sessionId)
{
	bool found = false;
	for(SessionState *s : _sessions) {
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

	connect(client, SIGNAL(disconnected(Client*)), this, SLOT(lobbyDisconnectedEvent(Client*)));

	(new LoginHandler(client, this))->startLoginProcess();
}

/**
 * @brief Handle the move of a client from the lobby to a session
 * @param session
 * @param client
 */
void SessionServer::moveFromLobby(SessionState *session, Client *client)
{
	logger::debug() << client << "moved from lobby to" << session;
	Q_ASSERT(_lobby.contains(client));
	_lobby.removeOne(client);

	// the session handles disconnect events from now on
	disconnect(client, SIGNAL(disconnected(Client*)), this, SLOT(lobbyDisconnectedEvent(Client*)));

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
void SessionServer::userDisconnectedEvent(SessionState *session)
{
	bool delSession = false;
	if(session->userCount()==0) {
		logger::debug() << session << "Last user left";

		bool hasSnapshot = session->mainstream().hasSnapshot();

		// A non-persistent session is deleted when the last user leaves
		// A persistent session can also be deleted if it doesn't contain a snapshot point.
		if(!hasSnapshot || !session->isPersistent()) {
			if(hasSnapshot)
				logger::info() << session << "Closing non-persistent session";
			else
				logger::info() << session << "Closing persistent session due to lack of snapshot point!";

			logger::info() << session << "History size was" << session->mainstream().totalLengthInBytes() << "bytes";
			delSession = true;
		}

		// If the hibernatable flag is set, it means we want to put the session
		// into storage as soon as possible
		delSession |= session->isHibernatable();
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

		QList<SessionState*> expirelist;

		for(SessionState *s : _sessions) {
			if(s->userCount()==0) {
				if(s->lastEventTime().msecsTo(now) > _expirationTime) {
					expirelist << s;
				}
			}
		}

		for(SessionState *s : expirelist) {
			logger::info() << s << "Vacant session expired. Uptime was" << s->uptime();

			if(_store && _store->autoStore() && s->isPersistent())
				s->setHibernatable(true);

			destroySession(s);
		}
	}
}

void SessionServer::refreshSessionAnnouncements()
{
	for(SessionState *s : _sessions) {
		if(s->publicListing().listingId>0) {
			_publicListingApi->refreshSession(s->publicListing(), {
				QString(),
				0,
				QString(),
				QString(),
				s->title(),
				s->userCount(),
				s->passwordHash().isEmpty() && !s->isPrivateUserList() ? s->userNames() : QStringList(),
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
	SessionState *s = getSessionById(listing.id);
	if(!s) {
		logger::warning() << "Announced non-existent session" << listing.id;
		return;
	}

	s->setPublicListing(listing);
}

void SessionServer::setWelcomeMessage(const QString &message)
{
	_welcomeMessage = message;
	for(SessionState *s : _sessions)
		s->setWelcomeMessage(message);
}

}
