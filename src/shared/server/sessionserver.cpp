/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "sessionserver.h"
#include "session.h"

#include "../util/logger.h"

namespace server {

SessionServer::SessionServer(QObject *parent)
	: QObject(parent),
	_nextId(1),
	_sessionLimit(1),
	_historyLimit(0),
	_allowPersistentSessions(false)
{
}

SessionServer::~SessionServer()
{
	if(!_sessions.isEmpty()) {
		logger::warning() << "Destroying" << _sessions.size() << "sessions...";
		QList<SessionState*> tbd = _sessions;
		for(SessionState *s : tbd)
			delete s;
	}
}

SessionState *SessionServer::createSession(int minorVersion)
{
	SessionState *session = new SessionState(_nextId++, minorVersion, allowPersistentSessions(), this);

	session->setHistoryLimit(_historyLimit);

	connect(session, SIGNAL(userConnected(SessionState*)), this, SIGNAL(sessionChanged(SessionState*)));
	connect(session, SIGNAL(userDisconnected(SessionState*)), this, SLOT(userDisconnectedEvent(SessionState*)));
	connect(session, SIGNAL(sessionAttributeChanged(SessionState*)), this, SIGNAL(sessionChanged(SessionState*)));
	connect(session, SIGNAL(destroyed(QObject*)), this, SLOT(sessionDeletedEvent(QObject*)));

	_sessions.append(session);

	emit sessionCreated(session);
	emit sessionChanged(session);

	logger::info() << "Session" << session->id() << "created";

	return session;
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
	int count = 0;
	for(const SessionState * s : _sessions)
		count += s->userCount();
	return count;
}

void SessionServer::stopAll()
{
	// TODO
	//session->kickAllUsers();
	//session->stopRecording();
}

void SessionServer::userDisconnectedEvent(SessionState *session)
{
	bool delSession = false;
	if(session->userCount()==0) {
		logger::debug() << "Last user of session" << session->id() << "left";

		bool hasSnapshot = session->mainstream().hasSnapshot();

		// A non-persistent session is deleted when the last user leaves
		// A persistent session can also be deleted if it doesn't contain a snapshot point.
		if(!hasSnapshot || !session->isPersistent()) {
			if(hasSnapshot)
				logger::info() << "Closing non-persistent session" << session->id();
			else
				logger::info() << "Closing session" << session->id() << "due to lack of snapshot point!";

			delSession = true;
		}
	}

	if(delSession)
		session->deleteLater();
	else
		emit sessionChanged(session);
}

void SessionServer::sessionDeletedEvent(QObject *session)
{
	SessionState *s = static_cast<SessionState*>(session);
	logger::info() << "Session" << s->id() << "deleted.";
	_sessions.removeAll(s);
	emit sessionEnded(s->id());
}

}
