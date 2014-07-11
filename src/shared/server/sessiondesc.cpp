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

#include "sessiondesc.h"
#include "session.h"
#include "client.h"

namespace server {

UserDescription::UserDescription()
	: id(0), isOp(false), isLocked(false), isSecure(false)
{
}

UserDescription::UserDescription(const Client &client)
	: id(client.id()),
	  name(client.username()),
	  address(client.peerAddress()),
	  isOp(client.isOperator()),
	  isLocked(client.isUserLocked()),
	  isSecure(client.isSecure())
{
}

SessionDescription::SessionDescription()
	: protoMinor(0), userCount(0), maxUsers(0), title(QString()),
	  closed(false), persistent(false), hibernating(false), historySizeMb(0), historyLimitMb(0),
	  historyStart(0), historyEnd(0)
{
}

SessionDescription::SessionDescription(const SessionState &session, bool getExtended, bool getUsers)
	: id(session.id()),
	  protoMinor(session.minorProtocolVersion()),
	  userCount(session.userCount()),
	  maxUsers(session.maxUsers()),
	  title(session.title()),
	  passwordHash(session.passwordHash()),
	  founder(session.founder()),
	  closed(session.isClosed()),
	  persistent(session.isPersistent()),
	  hibernating(false),
	  startTime(session.sessionStartTime()),
	  historySizeMb(0),
	  historyLimitMb(session.historyLimit()),
	  historyStart(0),
	  historyEnd(0)
{
	if(getUsers) {
		for(const Client *c : session.clients())
			users.append(UserDescription(*c));
	}

	if(getExtended) {
		historySizeMb = session.mainstream().lengthInBytes() / qreal(1024*1024);
		historyStart = session.mainstream().offset();
		historyEnd = session.mainstream().end();
	}
}

}
