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
#ifndef SESSIONSTORE_H
#define SESSIONSTORE_H

#include <QObject>
#include <QList>

namespace server {

class SessionDescription;
class SessionState;

/**
 * @brief An abstract base class for session hibernation implementations
 */
class SessionStore : public QObject
{
	Q_OBJECT
public:
	explicit SessionStore(QObject *parent=0);
	virtual ~SessionStore() = default;

	/**
	 * @brief Set whether all sessions should be stored on exit
	 *
	 * Normally, only persistent session will be stored
	 *
	 * @param storeall
	 */
	void setStoreAllSessions(bool storeall) { _storeAll = storeall; }
	bool storeAllSessions() const { return _storeAll; }

	/**
	 * @brief Set whether (persistent) sessions should be automatically stored instead of deleted
	 * @param autostore
	 */
	void setAutoStore(bool autostore) { _autoStore = autostore; }
	bool autoStore() const { return _autoStore; }

	//! Return a list of available sessions
	virtual QList<SessionDescription> sessions() const = 0;

	//! Get the description of a stored session
	virtual SessionDescription getSessionDescriptionById(const QString &id) const = 0;

	/**
	 * @brief Load a session with the given ID
	 *
	 * Loading a session removes it from the store.
	 *
	 * @param id session ID
	 * @return newly defrosted session or null if not found
	 */
	virtual SessionState *takeSession(const QString &id) =  0;

	/**
	 * @brief Put a session in non-volatile storage
	 * @param session
	 * @return false on error
	 */
	virtual bool storeSession(const SessionState *session) = 0;

	/**
	 * @brief Delete the session with the given ID from the store
	 * @param id
	 * @return true if a session was deleted
	 */
	virtual bool deleteSession(const QString &id) = 0;

signals:
	//! A new session has become available in the store
	void sessionAvailable(const SessionDescription &dsession);

private:
	bool _storeAll;
	bool _autoStore;
};

}

#endif // SESSIONSTORE_H
