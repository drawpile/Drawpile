/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#ifndef DP_SERVER_TPLLOADER_H
#define DP_SERVER_TPLLOADER_H

class QJsonArray;
class QJsonObject;
class QString;

namespace server {

class SessionHistory;

/**
 * @brief Abstract base class for server template loaders
 */
class TemplateLoader {
public:
	virtual ~TemplateLoader() = default;

	/**
	 * @brief Get a list of all available templates.
	 *
	 * The template description is a subset of a session description. Specifically,
	 * it includes the following attributes:
	 * 
	 *  - alias
	 *  - protocol
	 *  - maxUserCount
	 *  - founder
	 *  - hasPassword
	 *  - title
	 *  - nsfm
	 *
	 *  The following field are not present:
	 *
	 *   - id (templates are identified by alias only)
	 *   - startTime (templates are not live sessions)
	 */
	virtual QJsonArray templateDescriptions() const = 0;

	/**
	 * @brief Get the description for a specific template
	 * @param alias template ID alias
	 * @return empty object if template not found
	 */
	virtual QJsonObject templateDescription(const QString &alias) const = 0;

	/**
	 * @brief Check if a template with the given alias exists
	 */
	virtual bool exists(const QString &alias) const = 0;

	/**
	 * @brief Initialize the given session with a template
	 *
	 * This will set the session's initial content and attributes.
	 *
	 * @param session the session history to initialize
	 * @return false if session could not be initialized
	 */
	virtual bool init(SessionHistory *session) const = 0;
};

}

#endif

