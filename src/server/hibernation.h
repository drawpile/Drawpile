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
#ifndef HIBERNATION_H
#define HIBERNATION_H

#include "../shared/server/sessionstore.h"
#include "../shared/server/sessiondesc.h"

namespace server {

class SessionState;

class Hibernation : public SessionStore
{
	Q_OBJECT
public:
	explicit Hibernation(const QString &sessionDirectory, QObject *parent=0);

	/**
	 * @brief Initialize session storage
	 *
	 * This function will try to make sure the session storage directory exists
	 * and scans it for any existing sessions. This should be called before
	 * passing this object to the session server.
	 *
	 * @return false if session store is unavailable
	 */
	bool init();

	QList<SessionDescription> sessions() const { return _sessions; }

	SessionDescription getSessionDescriptionById(int id) const;

	SessionState *takeSession(int id);
	bool storeSession(const SessionState *session);

private:
	QList<SessionDescription> _sessions;
	QString _path;
};

}

#endif // HIBERNATION_H
