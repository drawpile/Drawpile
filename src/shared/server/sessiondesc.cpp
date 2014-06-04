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

#include "sessiondesc.h"
#include "session.h"

namespace server {

SessionDescription::SessionDescription()
	: id(0), protoMinor(0), userCount(0), title(QString()),
	  passwordProtected(false), closed(false), persistent(false), hibernating(false)
{
}

SessionDescription::SessionDescription(const SessionState &session)
	: id(session.id()),
	  protoMinor(session.minorProtocolVersion()),
	  userCount(session.userCount()),
	  title(session.title()),
	  passwordProtected(!session.password().isEmpty()),
	  closed(session.isClosed()),
	  persistent(session.isPersistent()),
	  hibernating(false)
{
}

}
