/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#ifndef ANNOUNCABLE_H
#define ANNOUNCABLE_H

class QString;

namespace sessionlisting {

struct Session;

/**
 * Interface class for announcable sessions
 */
class Announcable {
public:
	virtual ~Announcable();

	//! Get the ID of the announcable session
	virtual QString id() const = 0;

	//! Get an announcement for this session
	virtual Session getSessionAnnouncement() const = 0;

	//! Send a message to the users of this session
	virtual void sendListserverMessage(const QString &message) = 0;
};

}

#endif // ANNOUNCABLE_H
