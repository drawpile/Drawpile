// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_SERVER_TPLLOADER_H
#define DP_SERVER_TPLLOADER_H
#include <QJsonObject>
#include <QVector>

class QJsonArray;
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
	 * The template description is a subset of a session description.
	 * Specifically, it includes the following attributes:
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
	virtual QVector<QJsonObject>
	templateDescriptions(bool includeUnlisted) const = 0;

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

