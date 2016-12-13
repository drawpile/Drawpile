/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2016 Calle Laakkonen

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

QString sessionIdString(const QUuid &id)
{
	QString s = id.toString();
	return s.mid(1, s.length()-2);
}

bool isValidSessionAlias(const QString &alias)
{
	if(alias.length() > 32 || alias.length() < 1)
		return false;

	for(int i=0;i<alias.length();++i) {
		const QChar c = alias.at(i);
		if(!(
			(c >= 'a' && c<'z') ||
			(c >= 'A' && c<'Z') ||
			(c >= '0' && c<'9') ||
			c=='-'
			))
			return false;
	}

	// To avoid confusion with real session IDs,
	// aliases may not be valid UUIDs.
	if(!QUuid(alias).isNull())
		return false;

	return true;
}


SessionDescription::SessionDescription()
	: userCount(0), maxUsers(0), title(QString()),
	  closed(false), persistent(false), nsfm(false)
{
}

SessionDescription::SessionDescription(const Session &session)
	: id(session.id()),
	  alias(session.idAlias()),
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
