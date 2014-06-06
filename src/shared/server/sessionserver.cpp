/*
   DrawPile - a collaborative drawing program.

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

#include <QTimer>

namespace server {

SessionServer::SessionServer(QObject *parent)
	: QObject(parent),
	_store(nullptr),
	_nextId(1),
	_sessionLimit(1),
	_historyLimit(0),
	_expirationTime(0),
	_allowPersistentSessions(false),
	_mustSecure(false)
{
	QTimer *cleanupTimer = new QTimer(this);
	connect(cleanupTimer, SIGNAL(timeout()), this, SLOT(cleanupSessions()));
	cleanupTimer->setInterval(15 * 1000);
	cleanupTimer->start(cleanupTimer->interval());
}

void SessionServer::setSessionStore(SessionStore *store)
{
	Q_ASSERT(store);
	_store = store;
	store->setParent(this);

	for(const SessionDescription &s : _store->sessions()) {
		if(s.id >= _nextId)
			_nextId = s.id + 1;
	}

	connect(store, SIGNAL(sessionAvailable(SessionDescription)), this, SIGNAL(sessionChanged(SessionDescription)));
}

QList<SessionDescription> SessionServer::sessions() const
{
	QList<SessionDescription> descs;

	foreach(const SessionState *s, _sessions)
		descs.append(SessionDescription(*s));

	if(_store)
		descs += _store->sessions();

	return descs;
}

SessionState *SessionServer::createSession(int minorVersion)
{
	SessionState *session = new SessionState(_nextId++, minorVersion, this);

	initSession(session);

	logger::info() << *session << "created";

	return session;
}

void SessionServer::initSession(SessionState *session)
{
	session->setHistoryLimit(_historyLimit);
	session->setPersistenceAllowed(allowPersistentSessions());

	connect(session, &SessionState::userConnected, this, &SessionServer::moveFromLobby);
	connect(session, &SessionState::userDisconnected, this, &SessionServer::userDisconnectedEvent);
	connect(session, &SessionState::sessionAttributeChanged, [this](SessionState *ses) { emit sessionChanged(SessionDescription(*ses)); });

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

	logger::debug() << "Deleting" << *session << "with" << session->userCount() << "users";
	_sessions.removeOne(session);

	int id = session->id();

	if(session->isHibernatable() && _store) {
		logger::info() << "Hibernating" << *session;
		_store->storeSession(session);
	}

	session->deleteLater(); // destroySession call might be triggered by a signal emitted from the session
	emit sessionEnded(id);
}

SessionDescription SessionServer::getSessionDescriptionById(int id) const
{
	for(SessionState *s : _sessions) {
		if(s->id() == id)
			return SessionDescription(*s);
	}

	if(_store)
		return _store->getSessionDescriptionById(id);

	return SessionDescription();
}

SessionState *SessionServer::getSessionById(int id)
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
			logger::info() << *session << "restored from hibernation";
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

void SessionServer::addClient(Client *client)
{
	client->setParent(this);

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
	logger::debug() << *client << "moved from lobby to" << *session;
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
		logger::debug() << "Last user of" << *session << "left";

		bool hasSnapshot = session->mainstream().hasSnapshot();

		// A non-persistent session is deleted when the last user leaves
		// A persistent session can also be deleted if it doesn't contain a snapshot point.
		if(!hasSnapshot || !session->isPersistent()) {
			if(hasSnapshot)
				logger::info() << "Closing non-persistent" << *session;
			else
				logger::info() << "Closing" << *session << "due to lack of snapshot point!";

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
			logger::info() << "Vacant" << *s << "expired. Uptime was" << s->uptime();

			if(_store && _store->autoStore() && s->isPersistent())
				s->setHibernatable(true);

			destroySession(s);
		}
	}
}

}
