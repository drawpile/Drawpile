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

#ifndef SESSIONS_INTERFACE_H
#define SESSIONS_INTERFACE_H

#include <tuple>

class QJsonArray;
class QString;

namespace protocol {
	class ProtocolVersion;
}

namespace server {

class Session;

/**
 * Interface for a class that can accept client logins
 */
class Sessions {
public:
	virtual ~Sessions();

	/**
	 * Get a list of session descriptions
	 */
	virtual QJsonArray sessionDescriptions() const = 0;

	/**
	 * Get a session with the given ID or alias
	 *
	 * @param id session ID or alias
	 * @param load if true, a session is loaded from template if it's not yet live
	 * @return session or nullptr if session was not active or couldn't be loaded
	 */
	virtual Session *getSessionById(const QString &id, bool loadTemplate) = 0;

	/**
	 * Create a new session
	 *
	 * In case of error, one of the following error codes may be returned:
	 * 
	 *  - idInuse     - a session with this ID or alias already exists
	 *  - badProtocol - this protocol version is not supported by this server
	 *  - closed      - this server is full or not accepting new sessions
	 *
	 * @param id session unique ID
	 * @param alias session alias (may be empty)
	 * @param protocolVersion session protocol version
	 * @param founder name of the user who created the session
	 *
	 * @return session, error string pair: if session is null, error string contains the error code
	 */
	virtual std::tuple<Session*, QString> createSession(const QString &id, const QString &alias, const protocol::ProtocolVersion &protocolVersion, const QString &founder) = 0;
};

}

#endif
