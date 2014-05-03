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

#include "../util/logger.h"

#include <QTimer>

namespace server {

SessionServer::SessionServer(QObject *parent)
	: QObject(parent),
	_nextId(1),
	_sessionLimit(1),
	_historyLimit(0),
	_expirationTime(0),
	_allowPersistentSessions(false)
{
	QTimer *cleanupTimer = new QTimer(this);
	connect(cleanupTimer, SIGNAL(timeout()), this, SLOT(cleanupSessions()));
	cleanupTimer->setInterval(15 * 1000);
	cleanupTimer->start(cleanupTimer->interval());
}

SessionState *SessionServer::createSession(int minorVersion)
{
	SessionState *session = new SessionState(_nextId++, minorVersion, allowPersistentSessions(), this);

	session->setHistoryLimit(_historyLimit);

	connect(session, SIGNAL(userConnected(SessionState*, Client*)), this, SLOT(moveFromLobby(SessionState*, Client*)));
	connect(session, SIGNAL(userConnected(SessionState*, Client*)), this, SIGNAL(sessionChanged(SessionState*)));
	connect(session, SIGNAL(userDisconnected(SessionState*)), this, SLOT(userDisconnectedEvent(SessionState*)));
	connect(session, SIGNAL(sessionAttributeChanged(SessionState*)), this, SIGNAL(sessionChanged(SessionState*)));

	_sessions.append(session);

	emit sessionCreated(session);
	emit sessionChanged(session);

	logger::info() << "Session" << session->id() << "created";

	return session;
}

void SessionServer::destroySession(SessionState *session)
{
	Q_ASSERT(_sessions.contains(session));

	logger::debug() << "Deleting" << *session << "with" << session->userCount() << "users";
	_sessions.removeOne(session);

	int id = session->id();
	session->deleteLater(); // destroySession call might be triggered by a signal emitted from the session
	emit sessionEnded(id);
}

SessionState *SessionServer::getSessionById(int id) const
{
	for(SessionState *s : _sessions) {
		if(s->id() == id)
			return s;
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

	for(SessionState *s : _sessions) {
		s->stopRecording();
		s->kickAllUsers();
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
	}

	if(delSession)
		destroySession(session);
	else
		emit sessionChanged(session);

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
			destroySession(s);
		}
	}
}

}
