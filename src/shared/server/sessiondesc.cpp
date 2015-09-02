/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2015 Calle Laakkonen

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

#include <QUuid>


namespace server {

SessionId SessionId::randomId()
{
	QString uuid = QUuid::createUuid().toString();

	return SessionId(
		uuid.mid(1, uuid.length()-2), // strip the { and } chars
		false
	);
}

SessionId SessionId::customId(const QString &id)
{
	return SessionId(id, true);
}

SessionDescription::SessionDescription()
	: userCount(0), maxUsers(0), title(QString()),
	  closed(false), persistent(false), nsfm(false)
{
}

SessionDescription::SessionDescription(const Session &session)
	: id(session.id()),
	  protocolVersion(session.protocolVersion()),
	  userCount(session.userCount()),
	  maxUsers(session.maxUsers()),
	  title(session.title()),
	  passwordHash(session.passwordHash()),
	  founder(session.founder()),
	  closed(session.isClosed()),
	  persistent(session.isPersistent()),
	  nsfm(session.isNsfm()),
	  startTime(session.sessionStartTime())
{
}

}
